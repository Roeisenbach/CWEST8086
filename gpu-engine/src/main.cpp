// CoreWars8086 GPU engine -- batch runner.
//
// Loads warriors from survivors/ and zombies/ next to the exe, runs N wars on
// the GPU (shaders/compute.spv), writes score.csv next to the exe, and prints
// the wall-clock time of the GPU phase.
//
//   engine.exe [numGames] [maxRounds]      (defaults: 5000  200000)
//
// Build (MSYS2 g++, Vulkan SDK in %VULKAN_SDK%):
//   g++ -O2 -std=c++17 -static -static-libgcc -static-libstdc++ src/main.cpp \
//       -I"%VULKAN_SDK%/include" -Llib -lvulkan -o engine.exe
//
// The first run compiles the shader in the driver (minutes on Maxwell) and
// caches it to pipeline_cache.bin; later runs start instantly.
//
// A chunk of GPU_GAMES games is resident on the GPU at once (arena = GPU_GAMES *
// 128 KB, device-local).  The shader runs as WORKERS persistent threads that
// pull games from a global atomic queue head and advance each by at most
// ROUND_BUDGET rounds, persisting per-game state in a device-local scratch
// buffer; a worker whose game finishes (or was already done) immediately grabs
// another instead of idling.  The host re-dispatches (resetting the queue head)
// until every gamesOut[i].endReason != 0.  ROUND_BUDGET bounds each dispatch so
// it dodges the Windows GPU watchdog (TDR) no matter how long a war runs.
// GPU_GAMES/WORKERS/ROUND_BUDGET are tuned for a ~4 GB Maxwell part; on a bigger
// card GPU_GAMES and WORKERS can go up. gameIn/gameOut/code/speedTable/queue are
// host-visible; arena + scratch are DEVICE_LOCAL.

#include <vulkan/vulkan.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using u32 = uint32_t;
using u64 = uint64_t;

// ------------------------------------------------------------------ config
static const int  MAX_WARRIORS     = 20;      // shader hard limit
static const u32  SLAB_BYTES       = 0x20000u;
// Sized from the device at startup rather than baked in for one card. Command
// line arguments 3..5 override them when you want to pin a value.
static int  GPU_GAMES    = 0;        // games resident per chunk; 0 = derive from VRAM
static u32  ROUND_BUDGET = 0u;       // rounds per dispatch; 0 = auto-tune while running
static u32  WORKER_COUNT = 0u;       // persistent workers; 0 = derive from device limits

// How long a single dispatch may take before Windows resets the driver. The
// real value lives in the registry (TdrDelay, seconds); 2 is the default when
// the key is absent. Dispatches are aimed well under it.
static double g_tdrSeconds = 2.0;
static double g_dispatchTarget = 0.5;      // seconds; a comfortable fraction of the above
static bool   g_autoBudget = true;
static bool   g_trace = false;          // CW_TRACE=1 -> per-dispatch timings
static int64_t g_firstSeed = 0;         // battle seed of game 0
static int64_t g_pairSeed  = 20260731;  // which combinations come up
static bool   g_digest = false;         // --digest: one line per game, for engine/gpuref
static std::string g_survDir, g_zombDir;  // --survivors/--zombies; default: next to the exe

// A run can be one lane of several: the games this process plays are the global
// indices g_seedPhase, +stride, +2*stride ... Both the battle seed and the
// combination are drawn from that global index, so two lanes splitting a run by
// parity cover it exactly once between them and neither has to be told what the
// other is doing.
static int64_t g_seedStride = 1;
static int64_t g_seedPhase  = 0;
static int     g_comboSize  = 4;          // groups drawn into each battle
// --scores: a machine-readable standings file, rewritten as games finish so a
// caller can watch the board move rather than waiting for the run.
static std::string g_scoresPath;
// COMBO_SIZE is now g_comboSize (--combo), so a caller can match the CPU lane.
static const u32  SCRATCH_UINTS_PER_GAME = 16u + MAX_WARRIORS * 32u;  // GAME_HDR + slots

// overridable on the command line:  engine.exe [numGames] [maxRounds]
static int  TOTAL_GAMES = 5000;
static u32  MAX_ROUNDS  = 200000u;            // Competition.MAX_ROUND
static const u32  ZOMBIE_SPEED     = 2;       // Options.zombieSpeed
static const u32  MAX_WARRIOR_SIZE = 512;     // WarriorRepository.MAX_WARRIOR_SIZE

enum WType { T_SURVIVOR = 0, T_SURVIVOR_1 = 1, T_SURVIVOR_2 = 2, T_ZOMBIE = 3, T_ZOMBIE_H = 4 };

// ------------------------------------------------------------------ GPU structs (std430)
struct GameIn {
    u32 seedLo, seedHi, numGroups, numWarriors, zombieSpeed, _p0, _p1, _p2;
    u32 wType[MAX_WARRIORS], wGroup[MAX_WARRIORS], wCodeOff[MAX_WARRIORS], wCodeLen[MAX_WARRIORS];
};
struct GameOut {
    u32 endReason, rounds, numSurvivorsAlive, numWarriorsAlive, aliveMask, _p0, _p1, _p2;
    u32 deathReason[MAX_WARRIORS], loadOffset[MAX_WARRIORS], ssSeg[MAX_WARRIORS],
        esSeg[MAX_WARRIORS], srcIndex[MAX_WARRIORS];
};
static_assert(sizeof(GameIn)  == 352, "GameIn std430 layout");
static_assert(sizeof(GameOut) == 432, "GameOut std430 layout");
struct PushConstants { u32 gameCount; u32 roundBudget; u32 maxRounds; };

// ------------------------------------------------------------------ warriors
struct Warrior { std::string name; std::vector<uint8_t> code; WType type; u32 codeOff = 0; };
struct Group   { std::string name; std::vector<int> members; bool isZombie = false; };

static std::vector<Warrior> g_warriors;
static std::vector<Group>   g_surv;         // survivor groups (+ zombie group appended), file order
static int                  g_zombieGroup = -1;
static std::vector<u32>     g_codeBlob4;    // packed 4 bytes / u32

static void die(const std::string& m) { throw std::runtime_error(m); }

// Windows resets a GPU whose dispatch runs longer than TdrDelay seconds. The
// value is per-machine, so it is read rather than assumed.
static double readTdrDelaySeconds() {
    HKEY k;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                      0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return 2.0;
    DWORD v = 0, sz = sizeof(v), type = 0;
    const bool ok = RegQueryValueExA(k, "TdrDelay", nullptr, &type, (LPBYTE)&v, &sz) == ERROR_SUCCESS
                    && type == REG_DWORD && v > 0;
    RegCloseKey(k);
    return ok ? (double)v : 2.0;
}

// everything (survivors/, zombies/, shaders/, score.csv, pipeline_cache.bin) is
// resolved next to the executable, not the working directory.
static fs::path g_exeDir;
static std::string atExe(const std::string& rel) { return (g_exeDir / rel).string(); }
static void initExeDir() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    g_exeDir = fs::path(std::string(buf, n)).parent_path();
}
#define VK_CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) die(std::string(#x) + " -> " + std::to_string(_r)); } while (0)

static bool ciLess(const std::string& a, const std::string& b) {
    return std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end(),
        [](char x, char y) { return std::tolower((unsigned char)x) < std::tolower((unsigned char)y); });
}

static std::vector<uint8_t> readFileBytes(const fs::path& p, size_t cap) {
    std::ifstream f(p, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() > cap) b.resize(cap);
    return b;
}

static std::vector<fs::path> listRegularFilesSorted(const fs::path& dir) {
    std::vector<fs::path> out;
    if (!fs::is_directory(dir)) return out;
    for (auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        std::string n = e.path().filename().string();
        if (n.find('.') != std::string::npos) continue;   // engine's fixFiles drops these
        out.push_back(e.path());
    }
    std::sort(out.begin(), out.end(),
              [](const fs::path& a, const fs::path& b) { return ciLess(a.filename().string(), b.filename().string()); });
    return out;
}

// mirrors WarriorRepository.readWarriorsFileFromPath
static void loadSurvivors(const fs::path& dir) {
    int openWarrior = -1;              // pending "...1" warrior, group not yet committed
    std::string openName;
    for (auto& p : listRegularFilesSorted(dir)) {
        std::string name = p.filename().string();
        auto bytes = readFileBytes(p, MAX_WARRIOR_SIZE);
        char last = name.empty() ? 0 : name.back();

        if (last == '1') {
            openWarrior = (int)g_warriors.size();
            openName = name.substr(0, name.size() - 1);
            g_warriors.push_back({name, bytes, T_SURVIVOR_1});
        } else if (last == '2') {
            if (openWarrior < 0) continue;         // orphan "2": engine would NPE; skip
            int w2 = (int)g_warriors.size();
            g_warriors.push_back({name, bytes, T_SURVIVOR_2});
            g_surv.push_back({openName, {openWarrior, w2}, false});
            openWarrior = -1;
        } else {
            int w = (int)g_warriors.size();
            g_warriors.push_back({name, bytes, T_SURVIVOR});
            g_surv.push_back({name, {w}, false});
        }
    }
    // a dangling "...1" with no "...2" is dropped, matching the engine.
}

static void loadZombies(const fs::path& dir) {
    auto files = listRegularFilesSorted(dir);
    if (files.empty()) return;
    Group g; g.name = "ZoMbIeS"; g.isZombie = true;
    for (auto& p : files) {
        std::string name = p.filename().string();
        auto bytes = readFileBytes(p, MAX_WARRIOR_SIZE);
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](char c) { return (char)std::tolower((unsigned char)c); });
        WType t = (!lower.empty() && lower.back() == 'h') ? T_ZOMBIE_H : T_ZOMBIE;
        g.members.push_back((int)g_warriors.size());
        g_warriors.push_back({name, bytes, t});
    }
    g_zombieGroup = (int)g_surv.size();
    g_surv.push_back(std::move(g));
}

static void packCodeBlob() {
    std::vector<uint8_t> flat;
    for (auto& w : g_warriors) {
        w.codeOff = (u32)flat.size();
        flat.insert(flat.end(), w.code.begin(), w.code.end());
    }
    flat.resize((flat.size() + 3) & ~size_t(3), 0);
    g_codeBlob4.assign(std::max<size_t>(1, flat.size() / 4), 0);
    if (!flat.empty()) std::memcpy(g_codeBlob4.data(), flat.data(), flat.size());
}

// ------------------------------------------------------------------ pairing
// java.util.Random, exactly -- the same generator the engine itself uses, so
// the draw below can be reproduced anywhere.
struct JRandom {
    u64 s;
    explicit JRandom(int64_t seed) { s = (u64)(seed ^ 0x5DEECE66DLL) & ((1ULL << 48) - 1); }
    int32_t next(int bits) {
        s = (s * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
        return (int32_t)(s >> (48 - bits));
    }
    int32_t nextInt(int32_t bound) {
        int32_t r = next(31), m = bound - 1;
        if ((bound & m) == 0) return (int32_t)(((int64_t)bound * (int64_t)r) >> 31);
        for (int32_t u = r; u - (r = u % bound) + m < 0; u = next(31)) {}
        return r;
    }
};

// Which groups fight game k. A copy of cw::drawCombo in the CPU engine
// (src/engine/compete.cpp) -- the two must agree, and tests/gpuref.cpp exists to
// prove that they do. Being a pure function of (pairingSeed, k) is the point:
// the CPU and GPU lanes can split a run by seed parity without either of them
// having to be told which groups the other used.
static void drawCombo(int64_t pairingSeed, int k, int fieldSize, int comboSize, int* out) {
    if (comboSize > fieldSize) comboSize = fieldSize;
    u64 z = (u64)pairingSeed + (u64)(int64_t)k * 0x9E3779B97F4A7C15ULL;
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    JRandom rnd((int64_t)z);
    for (int i = 0; i < fieldSize; ++i) out[i] = i;
    for (int i = 0; i < comboSize; ++i) {
        const int j = i + rnd.nextInt(fieldSize - i);
        const int t = out[i]; out[i] = out[j]; out[j] = t;
    }
}

// ------------------------------------------------------------------ game build
struct BuiltGame { GameIn in; std::vector<int> slotToWarrior; };   // slot -> global warrior index

// Which game of the whole tournament our i'th game is. With the default stride
// of 1 and phase of 0 this is the identity, so nothing about a single-lane run
// changes.
static int64_t globalIndex(u32 i) { return g_seedPhase + (int64_t)i * g_seedStride; }

static BuiltGame buildGame(u32 gameIdx) {
    BuiltGame bg{};
    GameIn& gi = bg.in;
    const int64_t g    = globalIndex(gameIdx);
    const int64_t seed = g_firstSeed + g;
    gi.seedLo = (u32)(u64)seed; gi.seedHi = (u32)((u64)seed >> 32);
    gi.zombieSpeed = ZOMBIE_SPEED;

    int nSurvGroups = (int)g_surv.size() - (g_zombieGroup >= 0 ? 1 : 0);
    const int combo = (g_comboSize > 0) ? g_comboSize : nSurvGroups;
    std::vector<int> order((size_t)std::max(1, nSurvGroups));
    drawCombo(g_pairSeed, (int)g, nSurvGroups, combo, order.data());
    const int take = std::min(combo, nSurvGroups);
    std::vector<int> pick(order.begin(), order.begin() + take);
    if (g_zombieGroup >= 0) pick.push_back(g_zombieGroup);

    u32 slot = 0, gid = 0;
    for (int groupIdx : pick) {
        const Group& g = g_surv[groupIdx];
        for (int wi : g.members) {
            if (slot >= (u32)MAX_WARRIORS) break;          // cap (only trailing zombies drop)
            const Warrior& w = g_warriors[wi];
            gi.wType[slot]    = (u32)w.type;
            gi.wGroup[slot]   = gid;
            gi.wCodeOff[slot] = w.codeOff;
            gi.wCodeLen[slot] = (u32)w.code.size();
            bg.slotToWarrior.push_back(wi);
            ++slot;
        }
        ++gid;
    }
    gi.numGroups   = gid;
    gi.numWarriors = slot;
    return bg;
}

// ------------------------------------------------------------------ Vulkan
struct Vk {
    VkInstance instance{};
    VkPhysicalDevice phys{};
    VkDevice dev{};
    VkQueue queue{};
    u32 qfamily = 0;
    VkDescriptorSetLayout dsl{};
    VkPipelineLayout pl{};
    VkPipeline pipe{};
    VkPipelineCache pcache{};
    VkDescriptorPool dpool{};
    VkDescriptorSet dset{};
    VkCommandPool cpool{};
    VkCommandBuffer cmd{};
    VkFence fence{};
} vk;

struct Buf { VkBuffer buf{}; VkDeviceMemory mem{}; void* map = nullptr; VkDeviceSize size = 0; };

static u32 findMemType(u32 bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(vk.phys, &mp);
    for (u32 i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    die("no suitable memory type");
    return 0;
}

// hostVisible=true -> mapped, for CPU-filled/read buffers.
// hostVisible=false -> DEVICE_LOCAL, GPU-only (arena/scratch); init via vkCmdFillBuffer.
static Buf createBuf(VkDeviceSize size, bool hostVisible) {
    Buf b; b.size = size;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(vk.dev, &bi, nullptr, &b.buf));
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(vk.dev, b.buf, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = hostVisible
        ? findMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : findMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(vk.dev, &ai, nullptr, &b.mem));
    VK_CHECK(vkBindBufferMemory(vk.dev, b.buf, b.mem, 0));
    if (hostVisible) VK_CHECK(vkMapMemory(vk.dev, b.mem, 0, size, 0, &b.map));
    return b;
}

static std::vector<char> readSpv(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) die("cannot open " + path);
    size_t n = (size_t)f.tellg();
    std::vector<char> data(n);
    f.seekg(0); f.read(data.data(), n);
    return data;
}

static void initVulkan() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "corewars-gpu";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VK_CHECK(vkCreateInstance(&ici, nullptr, &vk.instance));

    u32 n = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &n, nullptr));
    if (!n) die("no Vulkan device");
    std::vector<VkPhysicalDevice> devs(n);
    VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &n, devs.data()));
    vk.phys = devs[0];
    for (auto d : devs) {
        VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(d, &p);
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { vk.phys = d; break; }
    }
    VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(vk.phys, &pp);
    std::printf("GPU: %s\n", pp.deviceName);

    // ---- size the run to this card ------------------------------------
    // Every resident game needs a 128 KB arena slab plus its scratch block, both
    // device-local, so the chunk is whatever the largest device-local heap can
    // hold with room to spare for the driver's own allocations.
    {
        VkPhysicalDeviceMemoryProperties mp;
        vkGetPhysicalDeviceMemoryProperties(vk.phys, &mp);
        VkDeviceSize localHeap = 0;
        for (u32 i = 0; i < mp.memoryHeapCount; ++i)
            if (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                localHeap = std::max(localHeap, mp.memoryHeaps[i].size);
        if (!localHeap) localHeap = 256ull << 20;          // integrated part, be modest

        const VkDeviceSize perGame = SLAB_BYTES + (VkDeviceSize)SCRATCH_UINTS_PER_GAME * 4u;
        const VkDeviceSize budget  = (VkDeviceSize)((double)localHeap * 0.55);
        VkDeviceSize fit = budget / perGame;

        // and never ask for a buffer bigger than the device will bind
        const VkDeviceSize maxBuf = pp.limits.maxStorageBufferRange;
        fit = std::min(fit, (VkDeviceSize)(maxBuf / SLAB_BYTES));

        if (GPU_GAMES <= 0)
            GPU_GAMES = (int)std::max<VkDeviceSize>(16, std::min<VkDeviceSize>(fit, 65536));

        // One worker per two resident games load-balances without leaving lanes
        // idle, clamped to what the device will actually dispatch.
        if (WORKER_COUNT == 0u) {
            u64 w = (u64)std::max(64, GPU_GAMES / 2);
            const u64 maxInvoc = (u64)pp.limits.maxComputeWorkGroupCount[0] * 64ull;
            w = std::min<u64>(w, maxInvoc);
            // The shader is latency-bound, so it wants every warp it can get:
            // measured on a 640-lane Maxwell, 4096 workers beat 2048 by ~10%
            // and 8192 gained nothing more. Still capped, because very large
            // grids have been seen to upset the driver.
            w = std::min<u64>(w, 4096ull);
            WORKER_COUNT = (u32)((w + 63ull) / 64ull * 64ull);      // whole workgroups
        }
        std::printf("device-local heap %.1f GB -> %d resident games, %u workers\n",
                    (double)localHeap / (1024.0 * 1024.0 * 1024.0), GPU_GAMES, WORKER_COUNT);
    }

    u32 qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk.phys, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(vk.phys, &qn, qf.data());
    vk.qfamily = UINT32_MAX;
    for (u32 i = 0; i < qn; ++i) if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { vk.qfamily = i; break; }
    if (vk.qfamily == UINT32_MAX) die("no compute queue");

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = vk.qfamily; qci.queueCount = 1; qci.pQueuePriorities = &prio;

    VkPhysicalDeviceFeatures feats{};
    feats.shaderInt64 = VK_TRUE;                 // RNG uses 64-bit ints
    // A stray index into one of the storage buffers is otherwise a hang and a
    // lost device, which is impossible to localise. With this on the driver
    // clamps it instead, so a bug shows up as a wrong score rather than a reset
    // display driver. Costs a little bandwidth; worth it until the shader is
    // proven clean.
    {
        VkPhysicalDeviceFeatures have{};
        vkGetPhysicalDeviceFeatures(vk.phys, &have);
        if (have.robustBufferAccess) feats.robustBufferAccess = VK_TRUE;
        else std::printf("note: no robustBufferAccess on this device\n");
    }
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.pEnabledFeatures = &feats;
    VK_CHECK(vkCreateDevice(vk.phys, &dci, nullptr, &vk.dev));
    vkGetDeviceQueue(vk.dev, vk.qfamily, 0, &vk.queue);

    VkDescriptorSetLayoutBinding b[7]{};
    for (int i = 0; i < 7; ++i) {
        b[i].binding = (u32)i;
        b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 7; dslci.pBindings = b;
    VK_CHECK(vkCreateDescriptorSetLayout(vk.dev, &dslci, nullptr, &vk.dsl));

    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &vk.dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    VK_CHECK(vkCreatePipelineLayout(vk.dev, &plci, nullptr, &vk.pl));

    auto spv = readSpv(atExe("shaders/compute.spv"));
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spv.size();
    smci.pCode = reinterpret_cast<const u32*>(spv.data());
    VkShaderModule sm;
    VK_CHECK(vkCreateShaderModule(vk.dev, &smci, nullptr, &sm));
    // pipeline cache on disk -- the NVIDIA driver's first SPIR-V compile is slow
    // (minutes on Maxwell); subsequent runs reuse this and start instantly.
    std::vector<char> cacheData;
    { std::ifstream cf(atExe("pipeline_cache.bin"), std::ios::binary);
      if (cf) cacheData.assign((std::istreambuf_iterator<char>(cf)), std::istreambuf_iterator<char>()); }
    VkPipelineCacheCreateInfo pcci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    pcci.initialDataSize = cacheData.size();
    pcci.pInitialData = cacheData.empty() ? nullptr : cacheData.data();
    VK_CHECK(vkCreatePipelineCache(vk.dev, &pcci, nullptr, &vk.pcache));

    std::printf("compiling pipeline%s...\n", cacheData.empty() ? " (first run: this can take a few minutes)" : "");
    auto pc0 = std::chrono::steady_clock::now();

    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = sm;
    cpci.stage.pName = "main";
    cpci.layout = vk.pl;
    VK_CHECK(vkCreateComputePipelines(vk.dev, vk.pcache, 1, &cpci, nullptr, &vk.pipe));
    vkDestroyShaderModule(vk.dev, sm, nullptr);
    std::printf("pipeline ready (%.1f s)\n",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - pc0).count());

    size_t csz = 0;
    if (vkGetPipelineCacheData(vk.dev, vk.pcache, &csz, nullptr) == VK_SUCCESS && csz) {
        std::vector<char> out(csz);
        if (vkGetPipelineCacheData(vk.dev, vk.pcache, &csz, out.data()) == VK_SUCCESS) {
            std::ofstream of(atExe("pipeline_cache.bin"), std::ios::binary);
            of.write(out.data(), (std::streamsize)csz);
        }
    }

    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &ps;
    VK_CHECK(vkCreateDescriptorPool(vk.dev, &dpci, nullptr, &vk.dpool));
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = vk.dpool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &vk.dsl;
    VK_CHECK(vkAllocateDescriptorSets(vk.dev, &dsai, &vk.dset));

    VkCommandPoolCreateInfo cpci2{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci2.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci2.queueFamilyIndex = vk.qfamily;
    VK_CHECK(vkCreateCommandPool(vk.dev, &cpci2, nullptr, &vk.cpool));
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = vk.cpool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(vk.dev, &cbai, &vk.cmd));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(vk.dev, &fci, nullptr, &vk.fence));
}

static void bindDescriptors(const Buf& gin, const Buf& code, const Buf& arena,
                            const Buf& gout, const Buf& speed, const Buf& scratch, const Buf& queue) {
    const Buf* bufs[7] = {&gin, &code, &arena, &gout, &speed, &scratch, &queue};
    VkDescriptorBufferInfo bi[7];
    VkWriteDescriptorSet w[7];
    for (int i = 0; i < 7; ++i) {
        bi[i] = {bufs[i]->buf, 0, VK_WHOLE_SIZE};
        w[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w[i].dstSet = vk.dset;
        w[i].dstBinding = (u32)i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(vk.dev, 7, w, 0, nullptr);
}

static void submitAndWait() {
    VK_CHECK(vkEndCommandBuffer(vk.cmd));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &vk.cmd;
    VK_CHECK(vkResetFences(vk.dev, 1, &vk.fence));
    VK_CHECK(vkQueueSubmit(vk.queue, 1, &si, vk.fence));
    VK_CHECK(vkWaitForFences(vk.dev, 1, &vk.fence, VK_TRUE, UINT64_MAX));
}

// Reset a chunk's arena (0xCC in the battle segment of each slab, 0 elsewhere)
// and scratch, as its own submit so the big fill never rides with a compute
// dispatch (which could tip it over the Windows GPU watchdog).
static void initChunk(u32 nGames, const Buf& arena, const Buf& scratch) {
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkResetCommandBuffer(vk.cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(vk.cmd, &bi));
    // Three commands, whatever the chunk size. The battle segment has to read
    // 0xCC everywhere; the stack and shared blocks above it are zeroed by the
    // shader as it allocates them (see allocCore), which is what lets a chunk
    // be tens of thousands of games instead of a few thousand.
    vkCmdFillBuffer(vk.cmd, arena.buf, 0, (VkDeviceSize)nGames * SLAB_BYTES, 0xCCCCCCCCu);
    vkCmdFillBuffer(vk.cmd, scratch.buf, 0, (VkDeviceSize)nGames * SCRATCH_UINTS_PER_GAME * sizeof(u32), 0u);
    submitAndWait();
}

// One dispatch: WORKERS persistent threads pull games from the queue (queue.map[0])
// and advance each by up to ROUND_BUDGET rounds.
// `work` holds the indices of the games that still have running to do. Late in
// a run that is a handful out of thousands, and feeding the whole range instead
// made every worker pay an atomic and a game-header read per finished game just
// to find out it had nothing to do.
static void dispatchPass(const std::vector<u32>& work, u32 workers, const Buf& queue) {
    const u32 nGames = (u32)work.size();
    u32* q = (u32*)queue.map;
    q[0] = 0u;                                   // queue head for this pass
    std::memcpy(q + 1, work.data(), work.size() * sizeof(u32));

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkResetCommandBuffer(vk.cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(vk.cmd, &bi));

    vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.pipe);
    vkCmdBindDescriptorSets(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.pl, 0, 1, &vk.dset, 0, nullptr);
    PushConstants pc{nGames, ROUND_BUDGET, MAX_ROUNDS};
    vkCmdPushConstants(vk.cmd, vk.pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(vk.cmd, (workers + 63) / 64, 1, 1);
    submitAndWait();
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    g_trace = getenv("CW_TRACE") != nullptr;
    initExeDir();

    // Named options are pulled out first so the positional knobs keep working.
    {
        std::vector<char*> pos;
        pos.push_back(argv[0]);
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            if      (a == "--digest")                    g_digest = true;
            else if (a == "--seed"      && i + 1 < argc) g_firstSeed = atoll(argv[++i]);
            else if (a == "--pair-seed" && i + 1 < argc) g_pairSeed  = atoll(argv[++i]);
            else if (a == "--survivors" && i + 1 < argc) g_survDir   = argv[++i];
            else if (a == "--zombies"   && i + 1 < argc) g_zombDir   = argv[++i];
            else if (a == "--scores"    && i + 1 < argc) g_scoresPath = argv[++i];
            else if (a == "--combo"     && i + 1 < argc) g_comboSize  = std::max(0, atoi(argv[++i]));
            else if (a == "--seed-stride" && i + 1 < argc) g_seedStride = std::max(1LL, atoll(argv[++i]));
            else if (a == "--seed-phase"  && i + 1 < argc) g_seedPhase  = atoll(argv[++i]);
            else if (a == "--workers"   && i + 1 < argc) WORKER_COUNT = (u32)std::max(1, atoi(argv[++i]));
            else if (a == "--resident"  && i + 1 < argc) GPU_GAMES    = std::max(1, atoi(argv[++i]));
            else pos.push_back(argv[i]);
        }
        argc = (int)pos.size();
        for (int i = 0; i < argc; ++i) argv[i] = pos[(size_t)i];
    }

    if (argc > 1) TOTAL_GAMES = std::max(1, atoi(argv[1]));
    if (argc > 2) MAX_ROUNDS  = (u32)std::max(1, atoi(argv[2]));
    // Rounds per dispatch and worker count decide whether a dispatch finishes
    // inside the driver's watchdog, and the right values are per-GPU -- so they
    // are arguments rather than constants baked in for one card.
    if (argc > 3) { ROUND_BUDGET = (u32)std::max(1, atoi(argv[3])); g_autoBudget = false; }
    if (argc > 4) WORKER_COUNT = (u32)std::max(1, atoi(argv[4]));
    if (argc > 5) GPU_GAMES    = std::max(1, atoi(argv[5]));

    g_tdrSeconds = readTdrDelaySeconds();
    g_dispatchTarget = g_tdrSeconds * 0.25;        // a quarter of the watchdog
    if (ROUND_BUDGET == 0u) ROUND_BUDGET = 4u;     // deliberately timid; it climbs
    std::printf("watchdog %.0f s -> aiming %.2f s per dispatch, %s round budget\n",
                g_tdrSeconds, g_dispatchTarget, g_autoBudget ? "auto" : "fixed");
    try {
        loadSurvivors(g_survDir.empty() ? atExe("survivors") : fs::path(g_survDir));
        loadZombies (g_zombDir.empty() ? atExe("zombies")   : fs::path(g_zombDir));
        packCodeBlob();

        int nSurvGroups = (int)g_surv.size() - (g_zombieGroup >= 0 ? 1 : 0);
        std::printf("Loaded %zu warriors, %d survivor groups%s.\n",
                    g_warriors.size(), nSurvGroups, g_zombieGroup >= 0 ? " + zombies" : "");
        if (nSurvGroups < 1) die("no survivor groups found in ./survivors");

        initVulkan();

        Buf codeBuf = createBuf(std::max<VkDeviceSize>(4, (VkDeviceSize)g_codeBlob4.size() * 4), true);
        std::memcpy(codeBuf.map, g_codeBlob4.data(), g_codeBlob4.size() * 4);

        Buf speedBuf = createBuf(65536 * sizeof(u32), true);
        {
            u32* s = (u32*)speedBuf.map;
            s[0] = 0;
            for (int e = 1; e < 65536; ++e) {
                int v = 1 + (int)(std::log((double)e) / std::log(2.0));   // Java Math.log path
                s[e] = (u32)std::min(16, v);
            }
            // The shader no longer reads this table -- a global load per living
            // warrior per round -- and computes min(16, 1 + floor(log2 e))
            // directly instead. That is only safe while the two agree exactly,
            // so it is checked here rather than assumed: a build of the driver
            // whose log() rounds differently would be caught on startup.
            for (int e = 1; e < 65536; ++e) {
                int msb = 31 - __builtin_clz((unsigned)e);      // == findMSB(e)
                if (s[e] != (u32)std::min(16, msb + 1))
                    die("speed curve mismatch at energy " + std::to_string(e) +
                        ": table " + std::to_string(s[e]) +
                        " vs findMSB " + std::to_string(std::min(16, msb + 1)));
            }
        }

        u32 chunk = (u32)std::min(TOTAL_GAMES, GPU_GAMES);
        Buf ginBuf     = createBuf((VkDeviceSize)chunk * sizeof(GameIn), true);
        Buf goutBuf    = createBuf((VkDeviceSize)chunk * sizeof(GameOut), true);
        Buf arenaBuf   = createBuf((VkDeviceSize)chunk * SLAB_BYTES, false);          // DEVICE_LOCAL (~625 MB at 5000)
        Buf scratchBuf = createBuf((VkDeviceSize)chunk * SCRATCH_UINTS_PER_GAME * sizeof(u32), false); // DEVICE_LOCAL
        // head + one index per resident game
        Buf queueBuf   = createBuf((VkDeviceSize)(chunk + 16) * sizeof(u32), true);
        bindDescriptors(ginBuf, codeBuf, arenaBuf, goutBuf, speedBuf, scratchBuf, queueBuf);

        std::vector<double> warriorScore(g_warriors.size(), 0.0);
        std::vector<int>    warriorSurvived(g_warriors.size(), 0);
        int gamesScored = 0;

        // The standings so far, keyed by the file each warrior was read from --
        // that is the only name a caller can match back to its own warriors.
        // Written to a temporary and renamed, so a reader never sees half a file.
        // `passes` / `inFlight` / `budget` say where the GPU is between finished
        // games: on a slow (or power-parked) card the first war can take minutes
        // to end, and without this the lane looks dead until it does.
        auto writeScores = [&](double elapsed, unsigned passes, unsigned inFlight, unsigned budget) {
            if (g_scoresPath.empty()) return;
            const std::string tmp = g_scoresPath + ".part";
            {
                std::ofstream f(tmp);
                if (!f) return;
                f << "games " << gamesScored << " 0 " << elapsed << "\n";
                f << "pass " << passes << ' ' << inFlight << ' ' << budget << "\n";
                for (size_t i = 0; i < g_warriors.size(); ++i) {
                    const WType t = g_warriors[i].type;
                    if (t == T_ZOMBIE || t == T_ZOMBIE_H) continue;
                    f << "w " << g_warriors[i].name << ' '
                      << std::setprecision(17) << warriorScore[i] << ' '
                      << warriorSurvived[i] << "\n";
                }
            }
            std::error_code ec;
            fs::rename(tmp, g_scoresPath, ec);
        };

        auto scoreGame = [&](const BuiltGame& bgm, const GameOut& o) {
            ++gamesScored;
            if (o.numSurvivorsAlive == 0) return;
            const double sc = 1.0 / (double)o.numSurvivorsAlive;
            for (int slot = 0; slot < MAX_WARRIORS; ++slot) {
                if (!(o.aliveMask & (1u << slot))) continue;
                const u32 inp = o.srcIndex[slot];
                if (inp >= bgm.slotToWarrior.size()) continue;
                const int wi = bgm.slotToWarrior[inp];
                const WType t = g_warriors[wi].type;
                if (t == T_ZOMBIE || t == T_ZOMBIE_H) continue;
                warriorScore[wi] += sc;
                warriorSurvived[wi] += 1;
            }
        };

        auto t0 = std::chrono::steady_clock::now();

        for (int base = 0; base < TOTAL_GAMES; base += GPU_GAMES) {
            u32 n = (u32)std::min(GPU_GAMES, TOTAL_GAMES - base);

            std::vector<BuiltGame> built(n);
            GameIn* gin = (GameIn*)ginBuf.map;
            for (u32 i = 0; i < n; ++i) {
                built[i] = buildGame((u32)(base + (int)i));
                gin[i] = built[i].in;
            }
            std::memset(goutBuf.map, 0, (size_t)n * sizeof(GameOut));   // endReason 0 == not done

            auto b0 = std::chrono::steady_clock::now();
            initChunk(n, arenaBuf, scratchBuf);
            const GameOut* gout = (const GameOut*)goutBuf.map;

            // A dispatch that overruns the driver watchdog loses the device, and
            // how many rounds fit inside it depends entirely on the card. So the
            // budget starts tiny and is steered by how long each dispatch
            // actually took, rather than being a number picked for one GPU.
            //
            // It has to start tiny again for every chunk. By the end of a chunk
            // the budget has climbed to suit a handful of surviving games; a new
            // chunk begins with every game running, so carrying that number over
            // meant the first dispatch of chunk two did thousands of times the
            // work of the last dispatch of chunk one -- and lost the device.
            if (g_autoBudget) ROUND_BUDGET = 4u;
            u32 pass = 0;
            std::vector<double> dispatchMs;
            std::vector<u32> work(n);
            for (u32 i = 0; i < n; ++i) work[i] = i;
            double worstRecent = 0.0;
            u32 sinceAdjust = 0;
            for (;; ++pass) {
                const auto d0 = std::chrono::steady_clock::now();
                dispatchPass(work, WORKER_COUNT, queueBuf);
                const double dsecs =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - d0).count();

                // What each dispatch actually cost. On a laptop the GPU's clock
                // and power state wander by more than an order of magnitude
                // between identical dispatches, so the *fastest* pass is the
                // only honest measure of what the shader costs -- the slow ones
                // are measuring the driver, not the code.
                dispatchMs.push_back(dsecs * 1000.0);

                // Rebuild the worklist from what is actually still running, and
                // score whatever dropped off it on the way -- that is what makes
                // the standings move during a run rather than at the end of it.
                std::vector<u32> next;
                next.reserve(work.size());
                for (u32 gi : work) {
                    if (gout[gi].endReason == 0u) { next.push_back(gi); continue; }
                    scoreGame(built[gi], gout[gi]);
                }
                writeScores(std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - t0).count(),
                            pass + 1, (unsigned)next.size(), ROUND_BUDGET);

                if (g_trace && pass < 16)
                    std::printf("    pass %3u  %7.2f ms  budget %6u  %u/%u still running\n",
                                pass, dsecs * 1000.0, ROUND_BUDGET, (u32)next.size(), n);

                work.swap(next);
                if (work.empty()) break;

                if (g_autoBudget) {
                    // Steer on the WORST dispatch of a recent window, not the
                    // last one. On a laptop -- especially on battery, where the
                    // GPU sits in its lowest power state -- identical dispatches
                    // vary by two orders of magnitude, so a run of fast passes
                    // says nothing about whether the next one will overrun the
                    // watchdog. Losing the device costs the whole run, so the
                    // budget backs off on the worst case and grows by halves.
                    worstRecent = std::max(worstRecent, dsecs);
                    if (++sinceAdjust >= 8) {
                        if (worstRecent > g_dispatchTarget && ROUND_BUDGET > 1u)
                            ROUND_BUDGET = std::max(1u, ROUND_BUDGET / 2u);
                        else if (worstRecent < g_dispatchTarget * 0.25)
                            ROUND_BUDGET = std::min(MAX_ROUNDS,
                                                    ROUND_BUDGET + std::max(1u, ROUND_BUDGET / 2u));
                        worstRecent = 0.0;
                        sinceAdjust = 0;
                    }
                }
            }
            double bsecs = std::chrono::duration<double>(std::chrono::steady_clock::now() - b0).count();

            // --digest: everything the shader decided about each war, in the
            // format engine/gpuref prints for the CPU engine. Diffing the two
            // is what proves the GPU is still playing the same game -- the load
            // offsets alone are a fingerprint of the whole RNG sequence.
            if (g_digest) {
                for (u32 i = 0; i < n; ++i) {
                    const GameOut& o = gout[i];
                    std::printf("war %d seed %lld end %u rounds %u alive %u mask %05x",
                                base + (int)i,
                                (long long)(g_firstSeed + globalIndex((u32)(base + (int)i))),
                                o.endReason, o.rounds, o.numSurvivorsAlive, o.aliveMask);
                    std::printf(" load");
                    for (u32 s = 0; s < built[i].in.numWarriors; ++s)
                        std::printf(" %04x", o.loadOffset[s]);
                    std::printf(" death");
                    for (u32 s = 0; s < built[i].in.numWarriors; ++s)
                        std::printf(" %u", o.deathReason[s]);
                    std::printf("\n");
                }
            }

            std::printf("  games %d..%d  (%.2f s, %u passes)\n", base, base + (int)n - 1, bsecs, pass + 1);
            if (!dispatchMs.empty()) {
                std::sort(dispatchMs.begin(), dispatchMs.end());
                std::printf("    dispatch ms: min %.2f  p25 %.2f  median %.2f  max %.2f"
                            "   (budget %u, %u games)\n",
                            dispatchMs.front(), dispatchMs[dispatchMs.size() / 4],
                            dispatchMs[dispatchMs.size() / 2], dispatchMs.back(),
                            ROUND_BUDGET, n);
            }
        }

        auto t1 = std::chrono::steady_clock::now();
        double secs = std::chrono::duration<double>(t1 - t0).count();

        // score.csv  (mirrors WarriorRepository.saveScoresToFile)
        {
            std::ofstream f(atExe("score.csv"));
            f << "Groups:\n";
            for (auto& g : g_surv) {
                if (g.isZombie) continue;
                double gs = 0.0;
                for (int wi : g.members) gs += warriorScore[wi];
                f << g.name << "," << gs << "\n";
            }
            f << "\nWarriors:\n";
            for (auto& g : g_surv) {
                if (g.isZombie) continue;
                for (int wi : g.members) f << g_warriors[wi].name << "," << warriorScore[wi] << "\n";
            }
        }

        writeScores(secs, 0, 0, ROUND_BUDGET);

        std::printf("\nRan %d games in %.3f s  (%.0f games/s).  Wrote score.csv\n",
                    TOTAL_GAMES, secs, TOTAL_GAMES / secs);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nERROR: %s\n", e.what());
        if (std::string(e.what()).find("-4") != std::string::npos)
            std::fprintf(stderr,
                "  -4 is VK_ERROR_DEVICE_LOST: the driver reset the GPU mid-dispatch.\n"
                "  Pin the knobs to something gentler and try again:\n"
                "    engine.exe <games> <maxRounds> <roundBudget> <workers> <residentGames>\n"
                "  e.g.  engine.exe 256 2000 1 2048\n");
        return 1;
    }
}
