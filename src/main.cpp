// =============================================================================
//  CORE WARS 8086 - debugger / IDE shell  (Dear ImGui + GLFW + OpenGL3)
//
//  Layout
//  ------
//    +-----------------------------------------------------------+
//    | logo | File Edit View Run Help          |  -  []  X        |  custom title bar
//    +-----------------------------------------------------------+
//    | run / pause / step / reset   |  cycle - speed - counters   |  toolbar
//    +---------------+-------------------------------------------+
//    | WORKSPACE     | [ Editor ][ Arena ][ Leaderboard ]        |
//    |  Player   1 2 |                                           |
//    |  Player   1 2 |   ...docked panels, all tabbed together   |
//    +---------------+-------------------------------------------+
//    | status bar                                                |
//    +-----------------------------------------------------------+
//
//  The workspace is *not* a dockable window: it is drawn straight into the
//  main window, left of the dockspace, with a draggable splitter.
// =============================================================================

#define GLFW_EXPOSE_NATIVE_WIN32
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "engine/session.h"   // cw::GuiSession -- the real 8086 war engine
#include "engine/disasm.h"    // cw::disasm -- naming the instruction a cycle ran
#include "engine/asm.h"       // cw::assemble / encodeVariants
#include "engine/listing.h"   // machine code -> editable source
#include "engine/compete.h"   // cw::runCompetition -- the tournament runner
#include "engine/netpack.h"   // field / result packs for a shared tournament

// winsock2 has to be in before windows.h (which glfw3native pulls in), and
// windows.h has to be told not to bring the old winsock with it
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <unordered_map>
#include <random>
#include <set>
#include <array>
#include <memory>
#include <thread>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <exception>
#include <functional>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Palette
// -----------------------------------------------------------------------------
static inline ImVec4 Hex(unsigned rgb, float a = 1.0f)
{
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
}
static inline ImU32 HexU(unsigned rgb, float a = 1.0f)
{
    return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, (int)(a * 255.0f + 0.5f));
}

// The palette is live: the theme picker writes straight into it, so every
// Col::* read below picks the new colours up on the next frame.
// Test builds only: per-section times (frame parts, workspace opening),
// written to perf.txt beside the exe every three seconds (average and worst).
#ifdef CWEST_TEST_HOOKS
struct PerfAcc { double sum = 0, max = 0; int n = 0; };
static std::map<std::string, PerfAcc> g_Perf;
struct PerfScope
{
    const char* name; double t0;
    explicit PerfScope(const char* n) : name(n), t0(glfwGetTime()) {}
    ~PerfScope() { const double d = (glfwGetTime() - t0) * 1000.0; PerfAcc& a = g_Perf[name]; a.sum += d; a.n++; if (d > a.max) a.max = d; }
};
#define PERF_CAT2(a, b) a##b
#define PERF_CAT(a, b) PERF_CAT2(a, b)
#define PERF(name) PerfScope PERF_CAT(perfScope_, __LINE__)(name)
static void PerfAdd(const char* name, double t0)
{
    const double d = (glfwGetTime() - t0) * 1000.0;
    PerfAcc& a = g_Perf[name]; a.sum += d; a.n++; if (d > a.max) a.max = d;
}
#else
#define PERF(name) do {} while (0)
static void PerfAdd(const char*, double) {}
#endif

namespace Col {
    inline unsigned Bg0     = 0x0B0D12;   // app background, tab bar, strips
    inline unsigned Bg1     = 0x12151C;   // panels
    inline unsigned Bg2     = 0x1A1E28;   // frames / elevated
    inline unsigned Bg3     = 0x232836;   // hovered
    inline unsigned Bg4     = 0x2D3344;   // active
    inline unsigned Line    = 0x232835;   // borders / separators
    inline unsigned Text    = 0xE4E8F0;
    inline unsigned TextDim = 0x7D8798;
    inline unsigned Accent  = 0x5B8CFF;
    inline unsigned Accent2 = 0x36D3C0;
    inline unsigned Warn    = 0xF5B94F;
    inline unsigned Danger  = 0xF0555B;
    inline unsigned Ok      = 0x4CC38A;
    // The arena belongs to the theme too: the core nobody has written to yet,
    // and the lines between the cells once it is zoomed in far enough.
    inline unsigned ArenaCore = 0x171B24;
    inline unsigned ArenaGrid = 0x0B0D12;
}

// One slot per colour above, in the order the customizer lists them.
static const int kPalCount = 15;
static unsigned* PalSlot(int i)
{
    unsigned* t[kPalCount] = { &Col::Bg0, &Col::Bg1, &Col::Bg2, &Col::Bg3, &Col::Bg4,
                               &Col::Line, &Col::Text, &Col::TextDim,
                               &Col::Accent, &Col::Accent2, &Col::Warn, &Col::Danger, &Col::Ok,
                               &Col::ArenaCore, &Col::ArenaGrid };
    return t[i];
}
static const char* PalName(int i)
{
    static const char* n[kPalCount] = { "Background", "Panel", "Frame", "Hovered", "Active",
                                        "Border", "Text", "Dim text",
                                        "Accent", "Accent 2", "Warning", "Danger", "OK",
                                        "Arena", "Arena lines" };
    return n[i];
}

// Each theme: Bg0 Bg1 Bg2 Bg3 Bg4 Line / Text TextDim / Accent Accent2 Warn
// Danger Ok / ArenaCore ArenaGrid. The backgrounds step evenly from the
// outermost (Bg0) to the most raised (Bg4); the arena sits a shade off the
// panel so the core reads as its own surface, and its lines are the deepest
// background so they cut cleanly between the cells.
// The first four keep the positions the old presets had (Slate is now
// Graphite), so a saved themeIndex still names the same theme.
struct ThemeDef { const char* name; unsigned c[kPalCount]; };
static const ThemeDef kThemes[] = {
    { "Midnight",  { 0x0B0D12, 0x12151C, 0x1A1E28, 0x232836, 0x2D3344, 0x232835,
                     0xE4E8F0, 0x7D8798,
                     0x5B8CFF, 0x36D3C0, 0xF5B94F, 0xF0555B, 0x4CC38A,
                     0x171B24, 0x0B0D12 } },
    { "Paper",     { 0xEDF0F5, 0xFFFFFF, 0xF2F4F8, 0xE6EAF1, 0xD7DDE7, 0xDCE1E9,
                     0x1A1F2B, 0x667185,
                     0x2F6BEA, 0x0E9384, 0xC2700A, 0xD92D3A, 0x16A34A,
                     0xE4E8EF, 0xFFFFFF } },
    { "Amber CRT", { 0x0D0A06, 0x15110A, 0x1E180E, 0x2A2114, 0x372B1A, 0x2B2215,
                     0xF3DDB0, 0x9E865C,
                     0xFFB21E, 0x8AD7B1, 0xFFD36B, 0xFF6650, 0x9BD65A,
                     0x1A140B, 0x0D0A06 } },
    { "Graphite",  { 0x141619, 0x1B1E22, 0x24282E, 0x2E333A, 0x393F48, 0x2A2E35,
                     0xE7E9ED, 0x8A919C,
                     0x8C9CFF, 0x5FD4B4, 0xEBC165, 0xF06A6E, 0x6DCB91,
                     0x22262C, 0x141619 } },
    { "Ocean",     { 0x07111C, 0x0C1826, 0x132234, 0x1B2D44, 0x243954, 0x1A2B40,
                     0xDCE9F5, 0x7592AE,
                     0x3BA4FF, 0x2EE6C5, 0xFFC15E, 0xFF5F6D, 0x46D39A,
                     0x0F1E2F, 0x07111C } },
    { "Sand",      { 0xEDE7DC, 0xFBF8F2, 0xF3EEE5, 0xE8E1D4, 0xDAD1C0, 0xE2DACB,
                     0x2B2620, 0x7A6F60,
                     0xC2571A, 0x2C7A73, 0xB7791F, 0xC53030, 0x3D8B40,
                     0xE9E2D5, 0xFBF8F2 } },
};
static const int kThemeCount = (int)(sizeof(kThemes) / sizeof(kThemes[0]));

// The group's colour, lightened for the second survivor. The groups bar uses
// the same function, so a survivor's swatch there is the colour it wears in the
// arena rather than a flat group tint.
static unsigned SlotTint(unsigned base, int slot)
{
    if (slot != 1) return base;
    unsigned out = 0;
    for (int sh = 16; sh >= 0; sh -= 8)
    {
        const float v = (float)((base >> sh) & 0xFF) / 255.0f;
        out |= (unsigned)(ImMin(1.0f, v * 1.25f + 0.06f) * 255.0f + 0.5f) << sh;
    }
    return out;
}

// Distinct tints handed out to players in order.
static const unsigned kPlayerTints[] = {
    0x4C8DFF, 0xF2724B, 0x2FD4C0, 0xC77DFF, 0xF2B549, 0x53C08A, 0xFF6E9C, 0x6EE7F2
};

// -----------------------------------------------------------------------------
// 8086 syntax highlighting
// -----------------------------------------------------------------------------
const TextEditor::LanguageDefinition& ASM8086()
{
    static bool inited = false;
    static TextEditor::LanguageDefinition langDef;
    if (!inited)
    {
        static const char* const keywords[] = {
            "AAA","ADC","ADD","AND","CALL","CBW","CLC","CLD","CLI","CMC","CMP","CMPSB","CMPSW","CWD","DAA","DAS","DEC",
            "DIV","HLT","IDIV","IMUL","IN","INC","INT","INTO","IRET","JA","JAE","JB","JBE","JC","JCXZ","JE","JG","JGE",
            "JL","JLE","JMP","JNA","JNAE","JNB","JNBE","JNC","JNE","JNG","JNGE","JNL","JNLE","JNO","JNP","JNS","JNZ",
            "JO","JP","JPE","JPO","JS","JZ","LAHF","LDS","LEA","LES","LODSB","LODSW","LOOP","LOOPE","LOOPNE","LOOPNZ",
            "LOOPZ","MOV","MOVSB","MOVSW","MUL","NEG","NOP","NOT","OR","OUT","POP","POPA","POPF","PUSH","PUSHA","PUSHF",
            "RCL","RCR","REP","REPE","REPNE","REPNZ","REPZ","RET","RETF","ROL","ROR","SAHF","SAL","SAR","SBB","SCASB",
            "SCASW","SHL","SHR","STC","STD","STI","STOSB","STOSW","SUB","TEST","XCHG","XLATB","XOR",
            "DB","DW","DD","EQU","ORG","SEGMENT","ENDS","ASSUME","PROC","ENDP","END","BYTE","WORD","PTR","OFFSET"
        };
        for (auto& k : keywords)
            langDef.mKeywords.insert(k);

        static const char* const identifiers[] = {
            "AX","BX","CX","DX","AH","AL","BL","BH","CH","CL","DH","DL","DI","SI","BP","SP","DS","ES","SS","CS","IP","FLAGS"
        };
        for (auto& k : identifiers)
        {
            TextEditor::Identifier id;
            id.mDeclaration = "Built-in register";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("%[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Preprocessor));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[\\[\\]]", TextEditor::PaletteIndex::Preprocessor));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?0[xX][0-9a-fA-F]+|[0-9][0-9a-fA-F]*[hH]|\\$[0-9a-fA-F]+", TextEditor::PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?[01]+[bB]|[0-7]+[qQoO]", TextEditor::PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)", TextEditor::PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("'[^']*'|\"[^\"]*\"", TextEditor::PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[!%^&*()\\-+~|\\/,.:]", TextEditor::PaletteIndex::Punctuation));

        langDef.mSingleLineComment = ";";
        langDef.mCaseSensitive = false;
        langDef.mAutoIndentation = true;
        langDef.mName = "8086";

        // The tokenizer needs non-empty block-comment markers; \1 / \2 never occur in source.
        langDef.mCommentStart = "\1";
        langDef.mCommentEnd = "\2";

        inited = true;
    }
    return langDef;
}

// -----------------------------------------------------------------------------
// Model
// -----------------------------------------------------------------------------
static const int kSlotCount = 2;

struct Survivor
{
    std::string name;      // the file stem, e.g. "Phoenix1"
    // Both forms live in the workspace: the source the editor edits, and the
    // machine code that fights. Either file may not exist yet.
    std::string asmPath;   // <workspace>\assembly\<folder>\<name>.asm
    std::string binPath;   // <workspace>\survivors\<folder>\<name>
    unsigned long long asmTime = 0, binTime = 0;   // last-write times as last read or written here
    std::string code;      // assembly source (editor buffer)
    std::vector<unsigned char> bin;   // assembled machine code; non-empty => runnable
    std::vector<cw::AsmLine> asmLines;   // source line -> byte range
    std::vector<cw::AsmDiag> asmDiags;
    std::vector<cw::AsmSymbol> asmEqus;  // numeric EQUs and what they came to
    cw::Listing  listing;                // as first generated, for the warn flags
    std::vector<int> smartSites;         // where its source says int 0x89 (offset of the CD)
    bool        asmOk = true;
    bool        present = false;
    bool        enabled = true;          // "active": fights in Debug and Competition
    bool        dirty   = false;
    bool        runnable() const { return present && enabled && !bin.empty(); }
};

struct Player
{
    std::string name;
    Survivor    slots[kSlotCount];
    bool        enabled = true;          // "active": the group fights in Debug and Competition
    unsigned    tint = Col::Accent;
    // With twoTone off, survivor 2 is a lightened tint; with it on, tint2 is
    // its own colour and the two survivors read as separate warriors.
    unsigned    tint2 = Col::Accent;
    bool        twoTone = false;
    // The folder this group is filed under, as a path ("Bots/Old"); empty for
    // the top. On disk it is the same directory under survivors\ and assembly\.
    std::string folder;

    int LoadedCount() const { int n = 0; for (int i = 0; i < kSlotCount; i++) if (slots[i].present) n++; return n; }
    int FirstLoaded() const { for (int i = 0; i < kSlotCount; i++) if (slots[i].present) return i; return -1; }
};

// Zombies are single warriors that belong to no group and are never scored;
// they just make the arena hostile. One row each in the groups bar, no 1/2.
struct Zombie
{
    Survivor sv;                      // same editing / assembly machinery as a survivor
    std::string name;                 // display name, independent of the file it came from
    unsigned tint = 0xF2B549;
    int      speed = 2;               // cycles per turn, this zombie alone
    bool     heavy = false;           // ZOMBIE_H -- runs at double speed
    bool runnable() const { return sv.runnable(); }
};

// A folder of groups in the groups bar -- a directory in the workspace, nested
// as deep as you like. It owns nothing: a group belongs to the folder whose path
// it carries, so the list here is only which folders exist and which are shut.
struct GroupFolder
{
    std::string path;               // "Bots" or "Bots/Old"
    bool        collapsed = false;
};

// A row in the workspace. slot == -1 means the player row itself.
struct Ref
{
    int player = -1;
    int slot   = -2;
    bool Valid() const
    {
        if (player == -2 || player == -3) return slot >= 0;   // kZombiePlayer, kFolderPlayer
        return player >= 0 && slot >= -1 && slot < kSlotCount;
    }
};
static inline bool operator==(const Ref& a, const Ref& b) { return a.player == b.player && a.slot == b.slot; }
static inline bool operator!=(const Ref& a, const Ref& b) { return !(a == b); }

// Zombies live outside the player list but are edited exactly like survivors,
// so they take a slice of the same Ref space: player == kZombiePlayer and slot
// == the index into AppState::zombies.
static const int kZombiePlayer = -2;
static inline bool IsZombieRef(const Ref& r) { return r.player == kZombiePlayer && r.slot >= 0; }
// Folder rows can be selected too: player == kFolderPlayer, slot == the index
// into AppState::folders.
static const int kFolderPlayer = -3;
static inline bool IsFolderRef(const Ref& r) { return r.player == kFolderPlayer && r.slot >= 0; }

struct AppState
{
    std::vector<Player> players;
    std::vector<GroupFolder> folders;            // folders of groups, in display order
    std::vector<Ref>    selection;
    Ref                 anchor;                  // for shift-range selection
    Ref                 open;                    // what the editor currently shows

    TextEditor          editor;

    // --- live simulation ---
    cw::GuiSession      game;
    long long           gameSeed = 0;          // resolved from seedText when a war is armed
    char                seedText[64] = "guru"; // what the reference GUI starts with
    int                 zombieSpeed = 2;
    float               speedExp = 12.0f;         // cycles/sec = kSpeedBase ^ speedExp
    bool                speedReverse = false;     // "speed sign" -- reserved for Phase 3
    bool                debugStop = true;         // stop on events -- reserved for Phase 3

    int                 maxRounds = cw::War::MAX_ROUND;

    float               workspaceWidth = 268.0f;
    bool                showEditor = true, showArena = true, showBoard = true;
    bool                showAbout = false;
    float               arenaZoom = 3.0f;         // pixels per arena byte
    ImVec2              arenaPan = ImVec2(0, 0);
    double              tlLo = 0.0, tlHi = 0.0;   // visible cycle window on the timeline
    bool                showTimeline = true;
    bool                asmUpperCase = true;      // disassembly case
    bool                showAsmGutter = true;     // assembled bytes beside the code
    // Double-clicking a byte in the gutter re-cuts the listing from there to the
    // end of the code. Off by default: it rewrites the source.
    bool                dblClickDisasm = false;
    // Warriors routinely repoint SS / ES / CS. Each view decides for itself
    // whether to follow the register or stay on the block the survivor was
    // given; clicking the view's heading flips it.
    bool                followSS = false;
    bool                followES = false;
    bool                followCS = false;
    int                 arenaHoverOff = -1;       // arena byte under the cursor, or -1
    bool                arenaRulers = true;       // row / column offsets along the edges
    int                 inspectWarrior = -1;      // survivor shown in the side pane
    std::vector<int>    poppedInspectors;         // survivors torn off into windows
    bool                showInspector = true;
    bool                inspectorLeft = false;    // which side of the arena it takes
    float               inspectorWidth = 500.0f;
    bool                groupsCollapsed = false;  // names hidden, just the survivor strip
    // Panels and floating windows can be dragged out of the main window onto
    // the desktop, each in a window of its own (Dear ImGui's multi-viewports).
    bool                popOutWindows = false;
    // The arena's colours are the theme's (Col::ArenaCore / Col::ArenaGrid).
    float               arenaGridWidth = 1.0f;    // 0 = no lines, 1 = the usual hairline
    bool                advanceSeed = false;      // bump the seed on every recompute
    // --- competition ---
    cw::CompeteResult   compResult;
    cw::CompeteProgress compProg;
    bool                compRunning = false;
    int                 compGames   = 200;    // 0 = limitless
    int                 compThreads = 0;      // 0 = every core
    int                 compByGroup = 1;      // rank groups, or their survivors
    int                 compBackend = 0;      // 0 C++, 2 Busy (C++ + GPU), 3 Java (1, GPU alone, is gone)
    // "Busy (C++)" -- the C++ engine and the graphics card flat out together.
    // Off by default; Options turns it on, with a warning about the heat.
    bool                compAllowBusy = false;
    // The seed-search tool on the Competition toolbar.
    bool                compSeekTool = false;
    std::string         compGpuNote;          // why the GPU or Java lane is not running
    // Groups drawn into each battle. A war holds twenty warriors, so a field
    // bigger than about nine groups cannot all fight at once; four is what the
    // reference tournament uses. 0 means "the whole field".
    int                 compCombo   = 4;
    int                 compPairSeed = 20260731;   // which combinations come up
    // The seed the tournament's first war is fought from; war k uses this + k.
    // Empty means the Debug panel's seed, which is what it always used to be.
    char                compSeedText[64] = "";
    // How the tournament is shared out: 0 this machine, 2 host a run other
    // machines join, 3 join one. (1 was shard files carried between machines,
    // since removed; a saved 1 is read as 0.)
    int                 compMode    = 0;
    int                 compNetPort = 8086;
    char                compNetHost[128] = "";     // the host's https://....trycloudflare.com (or host:port here)
    char                compNetName[64]  = "";     // filled from the computer name
    // Who may join a hosted tournament: 0 anyone, 1 with the password (and then
    // every message is sealed with it), 2 whoever the host accepts. A joiner
    // types the same password to get into a protected host.
    int                 compNetProtect = 0;
    char                compNetPassword[64] = "";
    // Export: 0 .asm source, 1 binaries; survivors in subfolders like their
    // folders here, or side by side; the folder last exported to.
    int                 exportFormat = 0;
    bool                exportKeepFolders = false;
    char                exportDir[260] = "";
    // The bar graph: columns rising from the floor instead of bars from the
    // left, and squeezed to fit the panel (3 px a bar at most) with the stats
    // on hover, the way the timeline lanes work.
    bool                compBarsUp  = false;
    bool                compBarsFit = false;
    // Name and score written on each bar -- along it when the bars lie down,
    // turned on their side when they stand up as columns.
    bool                compBarLabels = true;
    // A drop shadow under the names written on the bars, so they read on
    // bright and dark bars alike.
    bool                compBarShadow = true;
    // What a run against folders shows for each folder: 0 the picked groups'
    // share of its points, 1 that share against an even split (100% = average,
    // share x the number of groups), 2 their place on its board.
    int                 compFieldShow = 0;
    // The group palette: five colours that groups made here take in turn
    // (groupPalNext). Groups read in from disk get a colour from their name
    // instead (DefaultGroupTint).
    unsigned            groupPal0 = 0xFF3B3B, groupPal1 = 0x2EDB4B, groupPal2 = 0x3D6BFF,
                        groupPal3 = 0xFFD83A, groupPal4 = 0xD54BFF;
    int                 groupPalNext = 0;
    // The tournament seed as a field on the Competition toolbar (only when the
    // panel has the room for it; Options always has it).
    bool                compSeedInBar = true;
    // Edits are written to disk shortly after the typing stops.
    bool                autoSave = true;

    // The edit tool: staged changes and whether a re-record is owed.
    bool                editMode = false;
    std::vector<cw::Poke> pokes;
    bool                pokesPending = false;
    int                 pokeRound = 0;

    std::vector<Zombie> zombies;

    // --- options -----------------------------------------------------------
    bool                showOptions = false;
    int                 themeIndex  = 0;
    // The open workspace (empty: none). Reopened on the next start.
    std::string         lastFolder;
    std::vector<std::string> recentWorkspaces;   // most recent first
    int                 folderSortMode = 0;      // 0 ask, 1 always sort
    bool                folderAssemble = true;   // assembly/*.asm  -> survivors/
    bool                folderDisassemble = true;// survivors/*     -> assembly/
    // breakpoints
    char                breakPatterns[128] = "";  // "CD 20, F4" -- hex, comma separated
    bool                breakStartsWith = false;  // false: bytes appear anywhere in the instruction
    bool                breakOnTimeline = true;
    // Pointer emphasis: the arena cell each of these registers points at gets a
    // ring in its own colour. The segment half is fixed in practice, so only the
    // offset matters -- see the note above ArenaPointerRings.
    unsigned            hlMask = 0;         // bit i = kPointerPairs[i]; nothing by default
    unsigned            hlColor = 0xF2B549;
    // Every second instruction around IP shaded, so instruction boundaries
    // read straight off the arena.
    bool                arenaInsnBands = false;
    int                 arenaInsnWho = 0;       // 0 the inspected survivor, 1 every live one
    int                 arenaInsnReach = 64;    // bytes each way from IP

    std::string         status = "Ready";
    ImFont*             fontUI = nullptr;
    ImFont*             fontMono = nullptr;
    float               fontUISize = 16.0f;
    float               fontMonoSize = 15.0f;
    float               fontEditorSize = 15.0f;   // the source editor alone (Ctrl +/-)
    // A file in the fonts folder; empty means the built-in choice.
    char                fontUIFile[160] = "";      // interface
    char                fontMonoFile[160] = "";    // code, hex, inspector, timeline
};
static AppState g_App;
static std::string g_StartWorkspace;     // the workspace open when the app last closed

// Defined further down (gui_workspace.inl, gui_options.inl, gui_compete.inl),
// but reached from the menus and the toolbar.
static void DrawZombieSection();
static void ZombieChip(int zi, float size);
static bool WriteSurvivorFiles(Survivor& sv);
static void NoteEdit();
static void FocusArenaOnIp(int wi = -1);
static void DrawOptionsWindow();
static void RunWsOps();
static void CompetitionStartFolders(const std::vector<int>& groups, const std::vector<std::string>& folders);

// A row of the groups bar by what it is rather than where it is, so it can be
// found again after the lists change. kind: 0 group (slot -1) or one of its
// survivors, 1 zombie (by file stem), 2 folder.
struct RefKey { int kind = -1; std::string folder, name; int slot = -1; };

// Changes asked for from inside the groups list -- a menu item, a drop, a key
// -- are carried out after the frame's panels are drawn, so the list is never
// rearranged while it is being walked.
struct WsOp
{
    enum Kind { None, OpenWorkspace, PickWorkspace, CloseWorkspace, MoveGroups, MoveFolder, Rename,
                NewFolder, NewGroup, NewZombie, Ungroup, Delete, Undo, Import, Paste, RunFolders, SortLoose };
    Kind                     kind = None;
    std::vector<RefKey>      items;     // what it acts on
    std::vector<std::string> paths;     // Import: files from outside; RunFolders: the folders
    std::string              folder;    // destination, or the folder acted on
    std::string              name;      // a new name; MoveFolder: the folder that moves
    bool                     zombies = false;   // Import / Paste into the zombie list
};
static std::vector<WsOp> g_WsOps;
static void QueueWsOp(const WsOp& op) { g_WsOps.push_back(op); }
static void QueueWsOp(WsOp::Kind k, const std::string& folder = std::string())
{
    WsOp op; op.kind = k; op.folder = folder; g_WsOps.push_back(op);
}

// The row whose name is being typed into (F2, or a new folder / group).
struct InlineRename
{
    bool   active = false;
    bool   focus = false;      // give the field the keyboard on its first frame
    RefKey key;
    char   buf[96] = "";
};
static InlineRename g_Rename;

// The groups bar had the last click, so the list keys (Ctrl+A, F2, Delete,
// Ctrl+Z, Ctrl+C) act on it.
static bool g_GroupsFocus = false;

// The warrior whose IP the arena keeps centred on (Follow IP), or -1.
static int  g_FollowIp = -1;

// Set by the timeline when its lanes need more height than the default
// strip gives them; read here on the next frame. 0 means the default.
static float g_TimelineWantH = 0.0f;

static float       g_Scale = 1.0f;   // DPI / UI scale
static HWND        g_Hwnd = nullptr;
static GLFWwindow* g_Window = nullptr;

// Frame driving. A frame is normally run from main()'s loop; while Windows
// holds its modal resize / move loop, the window procedure runs it instead.
static void AppFrame();
static void KeepFloatingWindowsClickable();
static bool g_AppReady    = false;   // ImGui and the app are up; frames may run
static bool g_InFrame     = false;   // guards against a frame inside a frame
static bool g_InSizeMove  = false;   // between WM_ENTERSIZEMOVE and WM_EXITSIZEMOVE
static bool g_LayoutBuilt = false;
static const UINT_PTR kSizeMoveTimer = 0xC0DE;

// -----------------------------------------------------------------------------
// Native title bar plumbing
// -----------------------------------------------------------------------------
static WNDPROC g_PrevWndProc = nullptr;
static float   g_TitleBarHeight = 40.0f;

// Rects (client pixels) inside the title bar that must NOT act as a drag handle.
static std::vector<ImVec4> g_NoDragBuild;    // filled while building the frame
static std::vector<ImVec4> g_NoDragActive;   // consumed by WM_NCHITTEST

static void AddNoDragRect(const ImVec2& mn, const ImVec2& mx)
{
    const ImVec2 origin = ImGui::GetMainViewport()->Pos;
    g_NoDragBuild.push_back(ImVec4(mn.x - origin.x, mn.y - origin.y, mx.x - origin.x, mx.y - origin.y));
}

static bool IsWindowMaximized(HWND hwnd)
{
    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd, &wp)) return false;
    return wp.showCmd == SW_SHOWMAXIMIZED;
}

static LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_NCCALCSIZE:
    {
        if (!wParam)
            break;

        // Give the whole window rect to the client area so we can paint our own
        // title bar. When maximized Windows inflates the requested rect by the
        // frame thickness; keeping all of it would push the top row of pixels
        // off-screen, so undo that inflation here.
        NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        RECT& rc = p->rgrc[0];
        if (IsWindowMaximized(hwnd))
        {
            const int fx = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            const int fy = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            rc.left   += fx;
            rc.right  -= fx;
            rc.top    += fy;
            rc.bottom -= fy;
        }
        return 0;
    }

    case WM_NCHITTEST:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);

        RECT rc;
        GetClientRect(hwnd, &rc);
        const int width  = rc.right;
        const int height = rc.bottom;
        const int border = (int)(6.0f * g_Scale + 0.5f);

        if (!IsWindowMaximized(hwnd))
        {
            const bool left   = pt.x < border;
            const bool right  = pt.x >= width - border;
            const bool top    = pt.y < border;
            const bool bottom = pt.y >= height - border;

            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left)            return HTLEFT;
            if (right)           return HTRIGHT;
            if (top)             return HTTOP;
            if (bottom)          return HTBOTTOM;
        }

        if (pt.y < (int)g_TitleBarHeight)
        {
            // Menus and window buttons live in the caption strip; they must get
            // real mouse input instead of starting a window drag.
            for (size_t i = 0; i < g_NoDragActive.size(); i++)
            {
                const ImVec4& r = g_NoDragActive[i];
                if (pt.x >= r.x && pt.x < r.z && pt.y >= r.y && pt.y < r.w)
                    return HTCLIENT;
            }
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = (LONG)(780 * g_Scale);
        mmi->ptMinTrackSize.y = (LONG)(480 * g_Scale);
        break;
    }

    case WM_ERASEBKGND:
        return 1; // the whole client area is repainted by OpenGL every frame

    // Dragging an edge (or the title strip) puts Windows into a modal loop of
    // its own: the main loop's glfwPollEvents does not return until the mouse
    // is let go, so nothing was drawn and the old frame was stretched. Draw
    // from here instead -- on every size change, and on a timer so a held
    // drag (or a running war) keeps moving.
    case WM_ENTERSIZEMOVE:
        g_InSizeMove = true;
        SetTimer(hwnd, kSizeMoveTimer, USER_TIMER_MINIMUM, NULL);
        break;
    case WM_EXITSIZEMOVE:
        g_InSizeMove = false;
        KillTimer(hwnd, kSizeMoveTimer);
        break;
    case WM_TIMER:
        if (wParam == kSizeMoveTimer)
        {
            if (g_InSizeMove && g_AppReady) AppFrame();
            return 0;
        }
        break;
    case WM_SIZE:
    {
        // GLFW learns the new size first, then the frame is drawn at it
        const LRESULT r = CallWindowProc(g_PrevWndProc, hwnd, msg, wParam, lParam);
        if (g_InSizeMove && g_AppReady && wParam != SIZE_MINIMIZED) AppFrame();
        return r;
    }
    }

    return CallWindowProc(g_PrevWndProc, hwnd, msg, wParam, lParam);
}

// -----------------------------------------------------------------------------
// Small file / path helpers
// -----------------------------------------------------------------------------
static bool FileExists(const std::string& path)
{
    std::ifstream f(path.c_str());
    return f.good();
}

// The folder the executable is in, with a trailing separator. Everything the
// app ships with or keeps (fonts, settings, layout) is found from here rather
// than from the working directory, which is wherever a shortcut or a command
// line happened to start it.
static std::string ExeDir()
{
    char exe[MAX_PATH] = "";
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    std::string p = exe;
    const size_t slash = p.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : p.substr(0, slash + 1);
}

static bool ReadTextFile(const std::string& path, std::string& out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    // normalise CRLF so the editor reports sane line numbers
    std::string norm;
    norm.reserve(out.size());
    for (size_t i = 0; i < out.size(); i++)
    {
        if (out[i] == '\r') { if (i + 1 < out.size() && out[i + 1] == '\n') continue; norm.push_back('\n'); }
        else norm.push_back(out[i]);
    }
    out.swap(norm);
    return true;
}

static bool WriteTextFile(const std::string& path, const std::string& text)
{
    std::ofstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    f << text;
    return true;
}

static std::string FileNameOf(const std::string& path)
{
    size_t p = path.find_last_of("\\/");
    return (p == std::string::npos) ? path : path.substr(p + 1);
}

static std::string StemOf(const std::string& fileName)
{
    size_t p = fileName.find_last_of('.');
    return (p == std::string::npos) ? fileName : fileName.substr(0, p);
}

static std::string ToLower(std::string s)
{
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

// "survivor1" -> ("survivor", 1) ; "warrior_2" -> ("warrior", 2) ; "alpha" -> ("alpha", 0)
static void SplitIndexedStem(const std::string& stem, std::string& base, int& index)
{
    base = stem;
    index = 0;
    size_t end = base.size();
    while (end > 0 && isdigit((unsigned char)base[end - 1])) end--;
    if (end < base.size() && end > 0)
    {
        index = atoi(base.c_str() + end);
        base = base.substr(0, end);
        while (!base.empty() && (base[base.size() - 1] == '_' || base[base.size() - 1] == '-' ||
                                 base[base.size() - 1] == ' ' || base[base.size() - 1] == '.'))
            base.erase(base.size() - 1);
    }
    if (base.empty()) base = stem;
}

static std::vector<std::string> OpenSurvivorDialog(HWND owner)
{
    std::vector<std::string> result;
    std::vector<char> buffer(64 * 1024, 0);

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = "Survivor sources (*.asm;*.s;*.txt)\0*.asm;*.s;*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrFile   = buffer.data();
    ofn.nMaxFile    = (DWORD)buffer.size();
    ofn.lpstrTitle  = "Load survivors  (select survivor1.asm + survivor2.asm for one player)";
    ofn.Flags       = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST |
                      OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

    if (!GetOpenFileNameA(&ofn))
        return result;

    const char* p = buffer.data();
    std::string first(p);
    p += first.size() + 1;

    if (*p == 0)
    {
        // single selection: lpstrFile is the full path
        result.push_back(first);
        return result;
    }

    std::string dir = first;
    if (!dir.empty() && dir[dir.size() - 1] != '\\') dir += '\\';
    while (*p)
    {
        result.push_back(dir + p);
        p += strlen(p) + 1;
    }
    return result;
}

// Same multi-select dialog, but for raw machine-code warrior files (no
// extension, as produced by the assembler / shipped in a survivors folder).
static std::vector<std::string> OpenBinaryDialog(HWND owner)
{
    std::vector<std::string> result;
    std::vector<char> buffer(64 * 1024, 0);

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = "All files (*.*)\0*.*\0";
    ofn.lpstrFile   = buffer.data();
    ofn.nMaxFile    = (DWORD)buffer.size();
    ofn.lpstrTitle  = "Load binary survivors  (select the assembled warrior files)";
    ofn.Flags       = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST |
                      OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

    if (!GetOpenFileNameA(&ofn))
        return result;

    const char* p = buffer.data();
    std::string first(p);
    p += first.size() + 1;
    if (*p == 0) { result.push_back(first); return result; }

    std::string dir = first;
    if (!dir.empty() && dir[dir.size() - 1] != '\\') dir += '\\';
    while (*p) { result.push_back(dir + p); p += strlen(p) + 1; }
    return result;
}

// -----------------------------------------------------------------------------
// Editor <-> model plumbing
// -----------------------------------------------------------------------------
static Survivor* SurvivorAt(const Ref& r)
{
    if (IsZombieRef(r))
        return (r.slot < (int)g_App.zombies.size()) ? &g_App.zombies[r.slot].sv : nullptr;
    if (r.player < 0 || r.player >= (int)g_App.players.size()) return nullptr;
    if (r.slot < 0 || r.slot >= kSlotCount) return nullptr;
    return &g_App.players[r.player].slots[r.slot];
}

// Name / tint / enabled state of whatever owns a row, for either kind of Ref.
static std::string OwnerNameOf(const Ref& r)
{
    if (IsZombieRef(r))
        return (r.slot < (int)g_App.zombies.size()) ? g_App.zombies[r.slot].name : std::string("Zombie");
    if (r.player >= 0 && r.player < (int)g_App.players.size()) return g_App.players[r.player].name;
    return "";
}
static unsigned OwnerTintOf(const Ref& r)
{
    if (IsZombieRef(r))
        return (r.slot < (int)g_App.zombies.size()) ? g_App.zombies[r.slot].tint : Col::Warn;
    if (r.player >= 0 && r.player < (int)g_App.players.size()) return g_App.players[r.player].tint;
    return Col::Accent;
}
static bool OwnerEnabledOf(const Ref& r)
{
    if (IsZombieRef(r)) return true;
    if (r.player >= 0 && r.player < (int)g_App.players.size()) return g_App.players[r.player].enabled;
    return true;
}

// -----------------------------------------------------------------------------
// Folders of groups
// -----------------------------------------------------------------------------
// Folder paths use '/' ("Bots/Old"); the empty path is the top.
static std::string FolderLeaf(const std::string& p)
{
    const size_t s = p.find_last_of('/');
    return s == std::string::npos ? p : p.substr(s + 1);
}
static std::string FolderParent(const std::string& p)
{
    const size_t s = p.find_last_of('/');
    return s == std::string::npos ? std::string() : p.substr(0, s);
}
static std::string FolderJoin(const std::string& parent, const std::string& leaf)
{
    return parent.empty() ? leaf : parent + "/" + leaf;
}
// p is root itself or somewhere inside it (everything is inside the top).
static bool FolderUnder(const std::string& p, const std::string& root)
{
    if (root.empty()) return true;
    const std::string a = ToLower(p), b = ToLower(root);
    return a == b || (a.size() > b.size() && a.compare(0, b.size(), b) == 0 && a[b.size()] == '/');
}
static int FolderDepth(const std::string& p)
{
    if (p.empty()) return 0;
    return 1 + (int)std::count(p.begin(), p.end(), '/');
}

// -----------------------------------------------------------------------------
// The folder tree, indexed. Every question below used to walk every group (or
// every folder) and lower-case each name on the way -- once per row drawn, so a
// workspace of a thousand groups cost tens of milliseconds a frame. The answers
// are now worked out in one pass and kept until the groups or folders change:
// a new frame, a different count, or a MarkFoldersChanged() from code that
// renames or moves something.
// -----------------------------------------------------------------------------
static int g_FrameNo = 0;            // AppFrame counts; the index never outlives a frame
// Bumped by whatever adds, removes, renames or moves a folder / a group.
static unsigned g_FolderGen = 1, g_PlayerGen = 1;
struct FolderIndexCache
{
    unsigned folderGen = 0, playerGen = 0;
    int    frame = -1;
    size_t nPlayers = 0, nFolders = 0;
    std::unordered_map<std::string, int>              index;     // lower path -> folder
    std::unordered_map<std::string, std::vector<int>> children;  // lower parent -> folders, by leaf
    std::unordered_map<std::string, std::vector<int>> inFolder;  // lower path ("" = loose) -> groups, load order
    std::unordered_map<std::string, std::vector<int>> byName;    // the same, by name
    std::unordered_map<std::string, std::vector<int>> under;     // lower path -> groups in it and below
};
static FolderIndexCache g_FolderIdx;

// a folder change can move groups too (their folder paths go with it)
static void MarkFoldersChanged() { g_FolderGen++; g_PlayerGen++; }
static void MarkPlayersChanged() { g_PlayerGen++; }

// Just path -> folder, which does not care about groups: loading a thousand
// groups adds a folder now and then but a group every time.
struct FolderPathCache
{
    unsigned gen = 0;
    int frame = -1;
    size_t n = 0;
    std::unordered_map<std::string, int> index;
};
static FolderPathCache g_FolderPaths;

static const std::unordered_map<std::string, int>& FolderPathIndex()
{
    FolderPathCache& c = g_FolderPaths;
    if (c.gen == g_FolderGen && c.frame == g_FrameNo && c.n == g_App.folders.size()) return c.index;
    c.index.clear();
    for (int i = 0; i < (int)g_App.folders.size(); i++) c.index.emplace(ToLower(g_App.folders[(size_t)i].path), i);
    c.gen = g_FolderGen;
    c.frame = g_FrameNo;
    c.n = g_App.folders.size();
    return c.index;
}

static const FolderIndexCache& FolderIdx()
{
    FolderIndexCache& c = g_FolderIdx;
    if (c.folderGen == g_FolderGen && c.playerGen == g_PlayerGen && c.frame == g_FrameNo &&
        c.nPlayers == g_App.players.size() && c.nFolders == g_App.folders.size())
        return c;
    c.index.clear(); c.children.clear(); c.inFolder.clear(); c.byName.clear(); c.under.clear();
    const int nf = (int)g_App.folders.size(), np = (int)g_App.players.size();
    std::vector<std::string> lowPath((size_t)nf), lowLeaf((size_t)nf);
    for (int i = 0; i < nf; i++)
    {
        lowPath[(size_t)i] = ToLower(g_App.folders[(size_t)i].path);
        lowLeaf[(size_t)i] = ToLower(FolderLeaf(g_App.folders[(size_t)i].path));
        c.index.emplace(lowPath[(size_t)i], i);      // the first of a duplicate wins, as the scan did
    }
    for (int i = 0; i < nf; i++)
        c.children[ToLower(FolderParent(g_App.folders[(size_t)i].path))].push_back(i);
    for (auto& kv : c.children)
        std::stable_sort(kv.second.begin(), kv.second.end(), [&](int a, int b) { return lowLeaf[(size_t)a] < lowLeaf[(size_t)b]; });
    std::vector<std::string> lowName((size_t)np);
    for (int p = 0; p < np; p++)
    {
        const Player& pl = g_App.players[(size_t)p];
        lowName[(size_t)p] = ToLower(pl.name);
        std::string f = ToLower(pl.folder);
        const bool known = !f.empty() && c.index.count(f);
        // its own folder's list even when that folder is not (or no longer)
        // one of ours, as the old scan answered; loose when it is not
        if (!f.empty()) c.inFolder[f].push_back(p);
        if (!known) { c.inFolder[std::string()].push_back(p); continue; }
        for (std::string up = f; !up.empty(); up = FolderParent(up)) c.under[up].push_back(p);
    }
    for (auto& kv : c.inFolder)
    {
        std::vector<int> v = kv.second;
        std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return lowName[(size_t)a] < lowName[(size_t)b]; });
        c.byName[kv.first] = std::move(v);
    }
    c.folderGen = g_FolderGen;
    c.playerGen = g_PlayerGen;
    c.frame = g_FrameNo;
    c.nPlayers = g_App.players.size();
    c.nFolders = g_App.folders.size();
    return c;
}

static std::vector<int> FolderIdxGet(const std::unordered_map<std::string, std::vector<int>>& m, const std::string& key)
{
    auto it = m.find(key);
    return it == m.end() ? std::vector<int>() : it->second;
}

static int FolderIndex(const std::string& path)
{
    if (path.empty()) return -1;
    const std::unordered_map<std::string, int>& idx = FolderPathIndex();
    auto it = idx.find(ToLower(path));
    return it == idx.end() ? -1 : it->second;
}

static void SortFolders()
{
    std::stable_sort(g_App.folders.begin(), g_App.folders.end(), [](const GroupFolder& a, const GroupFolder& b) {
        return ToLower(a.path) < ToLower(b.path);
    });
    MarkFoldersChanged();
}

// The folder, and every folder it is inside.
static int EnsureFolder(const std::string& path)
{
    if (path.empty()) return -1;
    int i = FolderIndex(path);
    if (i >= 0) return i;
    if (!FolderParent(path).empty()) EnsureFolder(FolderParent(path));
    GroupFolder f;
    f.path = path;
    g_App.folders.push_back(f);
    MarkFoldersChanged();
    return (int)g_App.folders.size() - 1;
}

// The groups filed directly in one folder, in load order. An empty path asks
// for the loose ones -- including any whose folder has since gone.
static std::vector<int> PlayersInFolder(const std::string& folder)
{
    return FolderIdxGet(FolderIdx().inFolder, ToLower(folder));
}

// The same, in the order the groups bar shows them: by name.
static std::vector<int> GroupsIn(const std::string& folder)
{
    return FolderIdxGet(FolderIdx().byName, ToLower(folder));
}

// Every group in a folder and in the folders inside it.
static std::vector<int> GroupsUnder(const std::string& folder)
{
    if (folder.empty()) return std::vector<int>();
    return FolderIdxGet(FolderIdx().under, ToLower(folder));
}

// The folders directly inside one, by name.
static std::vector<int> FolderChildren(const std::string& parent)
{
    return FolderIdxGet(FolderIdx().children, ToLower(parent));
}

// What a menu item or a drag on one group row acts on: every selected group
// when that row is among them, otherwise just the row itself.
static std::vector<int> GroupsForAction(int pi)
{
    std::vector<int> sel;
    bool among = false;
    for (const Ref& r : g_App.selection)
    {
        if (IsZombieRef(r) || r.player < 0 || r.player >= (int)g_App.players.size()) continue;
        if (std::find(sel.begin(), sel.end(), r.player) == sel.end()) sel.push_back(r.player);
        if (r.player == pi) among = true;
    }
    if (!among) return std::vector<int>(1, pi);
    std::sort(sel.begin(), sel.end());
    return sel;
}

static RefKey GroupKey(int p)
{
    RefKey k;
    if (p < 0 || p >= (int)g_App.players.size()) return k;
    k.kind = 0; k.folder = g_App.players[p].folder; k.name = g_App.players[p].name; k.slot = -1;
    return k;
}
static RefKey FolderKey(int fi)
{
    RefKey k;
    if (fi < 0 || fi >= (int)g_App.folders.size()) return k;
    k.kind = 2; k.folder = g_App.folders[fi].path;
    return k;
}

static void QueueFolderMove(const std::vector<int>& players, const std::string& folder)
{
    WsOp op;
    op.kind = WsOp::MoveGroups;
    for (int p : players) op.items.push_back(GroupKey(p));
    op.folder = folder;
    QueueWsOp(op);
}

#include "gui/gui_editor.inl"   // listing / re-assembly / assembled preview

// TextEditor::GetText() always appends one extra trailing newline; drop it so a
// freshly opened buffer does not immediately look modified.
static std::string EditorText()
{
    std::string t = g_App.editor.GetText();
    if (!t.empty() && t[t.size() - 1] == '\n') t.erase(t.size() - 1);
    return t;
}

// What the editor handed back the moment the open survivor was loaded into it.
// The editor normalises a few things on the way in (a missing final newline,
// stray whitespace at the end), so a file that was only opened can read back
// a byte or two different from disk; that is not an edit, and must not light
// the unsaved dot.
static std::string g_OpenBaseline;

// Autosave: anything that edits a survivor's text calls NoteEdit(); once the
// edits have stopped for a moment, whatever is unsaved is written out. One
// save per pause, so source that does not assemble yet is not rewritten every
// frame while it waits.
static double g_AutoSaveAt = -1.0;
static void NoteEdit() { g_AutoSaveAt = ImGui::GetTime() + 1.0; }

// The open survivor's code as it was when the editor was handed it. If the
// code changes underneath the editor (the folder loader swapping a binary's
// disassembly for the real source, say) while the editor still shows the old
// text untouched, the editor is the stale one -- flushing it back would write
// the disassembly over the source, and autosave would put that on disk.
static std::string g_OpenModelCode;

// Puts the open survivor's current code into the editor, if `s` is that one.
static void RefreshEditorIfOpen(const Survivor* s)
{
    if (!s || SurvivorAt(g_App.open) != s) return;
    g_App.editor.SetText(s->code);
    g_OpenBaseline = EditorText();
    g_OpenModelCode = s->code;
}

static void FlushEditorToModel()
{
    Survivor* s = SurvivorAt(g_App.open);
    if (!s || !s->present) return;
    const std::string text = EditorText();
    if (text == s->code) return;

    if (text == g_OpenBaseline)
    {
        // nothing has been typed since the editor was loaded
        if (s->code != g_OpenModelCode)
        {
            // ...but the code moved on without it: the code wins
            RefreshEditorIfOpen(s);
            return;
        }
        // only the editor's normalisation differs -- not an edit
        s->code = text;
        g_OpenModelCode = text;
        ReassembleSurvivor(*s);
        return;
    }
    s->code = text;
    s->dirty = true;
    NoteEdit();
    g_OpenBaseline = text;
    g_OpenModelCode = text;
    ReassembleSurvivor(*s);               // gutter + diagnostics follow the text
}

static void OpenInEditor(const Ref& r)
{
    Ref target = r;
    if (target.slot == -1 && target.player >= 0 && target.player < (int)g_App.players.size())
    {
        const int slot = IsZombieRef(target) ? target.slot
                                            : g_App.players[target.player].FirstLoaded();
        if (slot < 0)
        {
            FlushEditorToModel();
            g_App.open = Ref();
            g_App.editor.SetText("");
            g_App.editor.SetReadOnly(true);
            return;
        }
        target.slot = slot;
    }
    if (target == g_App.open) return;
    FlushEditorToModel();

    Survivor* s = SurvivorAt(target);
    if (!s || !s->present)
    {
        g_App.open = Ref();
        g_App.editor.SetText("");
        g_App.editor.SetReadOnly(true);
        return;
    }

    g_App.open = target;
    g_App.editor.SetReadOnly(false);
    g_App.editor.SetText(s->code);
    g_OpenBaseline = EditorText();
    g_OpenModelCode = s->code;
    PushEditorMarkers(*s);
    // A new file starts with nothing selected (the old selection used to
    // carry over -- and Ctrl+V over it deleted lines the new file lacks).
    g_App.editor.SetSelection(TextEditor::Coordinates(0, 0), TextEditor::Coordinates(0, 0));
    g_App.editor.SetCursorPosition(TextEditor::Coordinates(0, 0));
    g_App.status = "Opened " + OwnerNameOf(target) + " / " + s->name;
}

static void SaveSurvivor(const Ref& r)
{
    if (r == g_App.open) FlushEditorToModel();
    Survivor* s = SurvivorAt(r);
    if (!s || !s->present) return;
    if (!WriteSurvivorFiles(*s))                g_App.status = "FAILED to save " + s->name;
    else if (s->dirty)                          g_App.status = "Saved " + s->name + "'s source; its machine code "
                                                               "is kept as it was until the source assembles";
    else                                        g_App.status = "Saved " + s->name;
}

// Writes every survivor with unsaved changes; returns how many went to disk.
static int SaveDirty()
{
    FlushEditorToModel();
    int n = 0;
    auto saveOne = [&n](Survivor& sv) {
        if (!sv.present || !sv.dirty) return;
        if (WriteSurvivorFiles(sv)) n++;
    };
    for (int p = 0; p < (int)g_App.players.size(); p++)
        for (int s = 0; s < kSlotCount; s++)
            saveOne(g_App.players[p].slots[s]);
    for (Zombie& z : g_App.zombies)
        saveOne(z.sv);
    return n;
}

static void SaveAll()
{
    const int n = SaveDirty();
    char buf[64];
    snprintf(buf, sizeof(buf), "Saved %d file(s)", n);
    g_App.status = buf;
}

static void AutoSaveTick()
{
    if (!g_App.autoSave || g_AutoSaveAt < 0.0 || ImGui::GetTime() < g_AutoSaveAt) return;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;   // not in the middle of a drag
    g_AutoSaveAt = -1.0;
    const std::string before = g_App.status;
    const int n = SaveDirty();
    if (n > 0)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Autosaved %d file(s)", n);
        g_App.status = buf;
    }
    else if (n == 0) g_App.status = before;
}

// -----------------------------------------------------------------------------
// Selection
// -----------------------------------------------------------------------------
static std::vector<Ref> FlatRows()
{
    // In the order the groups bar draws them -- in each folder its folders
    // first, then its groups -- and without anything folded away, so a
    // shift-range selects exactly the rows between the two clicks and nothing
    // out of sight.
    struct Walk {
        static void in(const std::string& parent, std::vector<Ref>& rows)
        {
            for (int fi : FolderChildren(parent))
            {
                Ref r; r.player = kFolderPlayer; r.slot = fi; rows.push_back(r);
                if (!g_App.folders[fi].collapsed) in(g_App.folders[fi].path, rows);
            }
            for (int p : GroupsIn(parent))
            {
                Ref r; r.player = p; r.slot = -1; rows.push_back(r);
                for (int s = 0; s < kSlotCount; s++) { Ref q; q.player = p; q.slot = s; rows.push_back(q); }
            }
        }
    };
    std::vector<Ref> rows;
    Walk::in(std::string(), rows);
    return rows;
}

static bool IsSelected(const Ref& r)
{
    for (size_t i = 0; i < g_App.selection.size(); i++)
        if (g_App.selection[i] == r) return true;
    return false;
}

static void ToggleInSelection(const Ref& r)
{
    for (size_t i = 0; i < g_App.selection.size(); i++)
        if (g_App.selection[i] == r) { g_App.selection.erase(g_App.selection.begin() + i); return; }
    g_App.selection.push_back(r);
}

static void SelectRange(const Ref& a, const Ref& b)
{
    std::vector<Ref> rows = FlatRows();
    int ia = -1, ib = -1;
    for (int i = 0; i < (int)rows.size(); i++)
    {
        if (rows[i] == a) ia = i;
        if (rows[i] == b) ib = i;
    }
    if (ia < 0 || ib < 0) { g_App.selection.assign(1, b); return; }
    if (ia > ib) std::swap(ia, ib);
    g_App.selection.clear();
    for (int i = ia; i <= ib; i++) g_App.selection.push_back(rows[i]);
}

// Click handling shared by player rows and survivor buttons.
static void HandleRowClick(const Ref& r, bool openInEditor)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyShift && g_App.anchor.Valid())
    {
        SelectRange(g_App.anchor, r);
    }
    else if (io.KeyCtrl)
    {
        ToggleInSelection(r);
        g_App.anchor = r;
    }
    else
    {
        g_App.selection.assign(1, r);
        g_App.anchor = r;
    }
    if (openInEditor) OpenInEditor(r);
}

static bool RefEnabled(const Ref& r)
{
    if (IsZombieRef(r))
        return (r.slot < (int)g_App.zombies.size()) && g_App.zombies[r.slot].sv.enabled;
    if (IsFolderRef(r))
    {
        // a folder counts as active while any group in it is
        if (r.slot >= (int)g_App.folders.size()) return false;
        for (int p : GroupsUnder(g_App.folders[r.slot].path)) if (g_App.players[p].enabled) return true;
        return false;
    }
    if (r.player < 0 || r.player >= (int)g_App.players.size()) return false;
    const Player& p = g_App.players[r.player];
    if (r.slot == -1) return p.enabled;
    return p.slots[r.slot].enabled;
}

static void SetRefEnabled(const Ref& r, bool on)
{
    if (IsZombieRef(r))
    {
        if (r.slot < (int)g_App.zombies.size()) g_App.zombies[r.slot].sv.enabled = on;
        return;
    }
    if (IsFolderRef(r))
    {
        if (r.slot >= (int)g_App.folders.size()) return;
        for (int gp : GroupsUnder(g_App.folders[r.slot].path))
        {
            Ref g; g.player = gp; g.slot = -1;
            SetRefEnabled(g, on);
        }
        return;
    }
    if (r.player < 0 || r.player >= (int)g_App.players.size()) return;
    Player& p = g_App.players[r.player];
    // A group and its survivors stay consistent: turning the last one off turns
    // the group off, turning any back on brings the group back with it, and the
    // group itself carries its survivors either way.
    if (r.slot == -1)
    {
        p.enabled = on;
        for (int k = 0; k < kSlotCount; k++)
            if (p.slots[k].present) p.slots[k].enabled = on;
        return;
    }
    if (r.slot >= 0 && r.slot < kSlotCount && p.slots[r.slot].present)
    {
        p.slots[r.slot].enabled = on;
        if (on) p.enabled = true;
        else
        {
            bool any = false;
            for (int k = 0; k < kSlotCount; k++)
                if (p.slots[k].present && p.slots[k].enabled) any = true;
            if (!any) p.enabled = false;
        }
        return;
    }
    if (r.slot == -1) p.enabled = on;
    else              p.slots[r.slot].enabled = on;
}

static void ToggleRefEnabled(const Ref& r)
{
    SetRefEnabled(r, !RefEnabled(r));
    const char* state = RefEnabled(r) ? " is active" : " is out (not active)";
    if (IsZombieRef(r))
    {
        g_App.status = g_App.zombies[r.slot].name + state;
        return;
    }
    if (IsFolderRef(r))
    {
        g_App.status = FolderLeaf(g_App.folders[r.slot].path) + (RefEnabled(r) ? ": every group active"
                                                                              : ": every group out");
        return;
    }
    if (r.player < 0 || r.player >= (int)g_App.players.size()) return;
    const Player& p = g_App.players[r.player];
    g_App.status = (r.slot == -1) ? (p.name + state)
                                  : (p.slots[r.slot].name + state);
}

// Ctrl+Q: makes everything selected active -- or, when all of it already is,
// takes it all out.
static void SetSelectionActive(bool on)
{
    for (size_t i = 0; i < g_App.selection.size(); i++)
        SetRefEnabled(g_App.selection[i], on);
    char buf[96];
    snprintf(buf, sizeof(buf), "%d item(s) %s", (int)g_App.selection.size(), on ? "active" : "taken out");
    g_App.status = buf;
}

static void ToggleSelectionEnabled()
{
    if (g_App.selection.empty()) { g_App.status = "Nothing selected"; return; }
    bool allActive = true;
    for (size_t i = 0; i < g_App.selection.size(); i++)
        if (!RefEnabled(g_App.selection[i])) { allActive = false; break; }
    SetSelectionActive(!allActive);
}

// Only the selection fights: everything else is taken out.
static void ActivateOnlySelection()
{
    for (Player& pl : g_App.players)
    {
        pl.enabled = false;
        for (Survivor& sv : pl.slots) if (sv.present) sv.enabled = false;
    }
    for (Zombie& z : g_App.zombies) z.sv.enabled = false;
    for (const Ref& r : g_App.selection) SetRefEnabled(r, true);
    g_App.status = "Only the selection is active now";
}

// Ctrl+A in the groups bar: every folder, group and zombie.
static void SelectAllRows()
{
    g_App.selection.clear();
    for (int fi = 0; fi < (int)g_App.folders.size(); fi++) { Ref r; r.player = kFolderPlayer; r.slot = fi; g_App.selection.push_back(r); }
    for (int p = 0; p < (int)g_App.players.size(); p++)  { Ref r; r.player = p; r.slot = -1; g_App.selection.push_back(r); }
    for (int z = 0; z < (int)g_App.zombies.size(); z++)  { Ref r; r.player = kZombiePlayer; r.slot = z; g_App.selection.push_back(r); }
    if (!g_App.selection.empty()) g_App.anchor = g_App.selection.front();
    g_App.status = "Selected " + std::to_string((int)g_App.selection.size()) + " row(s)";
}

// -----------------------------------------------------------------------------
// Loading survivors
// -----------------------------------------------------------------------------
static const char* kTemplate =
    "; ---------------------------------------------------------\n"
    "; new survivor\n"
    "; ---------------------------------------------------------\n"
    "START:\n"
    "    MOV AX, 0\n"
    "    MOV CX, 100\n"
    "LOOP_BODY:\n"
    "    INC AX\n"
    "    DEC CX\n"
    "    JNZ LOOP_BODY\n"
    "    JMP START\n";

// ---- the group palette --------------------------------------------------------
static unsigned GroupPalette(int i)
{
    const unsigned* p[5] = { &g_App.groupPal0, &g_App.groupPal1, &g_App.groupPal2, &g_App.groupPal3, &g_App.groupPal4 };
    return *p[((i % 5) + 5) % 5];
}

static unsigned NameHash(const std::string& s)
{
    // FNV-1a, case and all: SilenTerror and SilentError are two survivors
    unsigned h = 2166136261u;
    for (unsigned char c : s) { h ^= (unsigned)c; h *= 16777619u; }
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;   // spread the bits
    return h;
}

// A group's colour the first time it is read in from disk, from its name: the
// first three characters pick one of the group tints (kPlayerTints), and the
// rest of the name shades it -- the same step on all three channels, so it is
// only ever lighter or darker, never another hue. The same name always gets
// the same colour.
static const int kShadeMax = 12;                      // RGB steps, either way

static unsigned ShadeRgb(const int c[3], int d)
{
    // all channels by the same amount, held back so none of them clips
    const int lo = ImMin(c[0], ImMin(c[1], c[2])), hi = ImMax(c[0], ImMax(c[1], c[2]));
    d = ImClamp(d, -lo, 255 - hi);
    return ((unsigned)(c[0] + d) << 16) | ((unsigned)(c[1] + d) << 8) | (unsigned)(c[2] + d);
}

static unsigned DefaultGroupTint(const std::string& name)
{
    const size_t n = sizeof(kPlayerTints) / sizeof(kPlayerTints[0]);
    const unsigned base = kPlayerTints[NameHash(name.substr(0, 3)) % (unsigned)n];
    const int c[3] = { (int)((base >> 16) & 0xFF), (int)((base >> 8) & 0xFF), (int)(base & 0xFF) };
    const unsigned r = NameHash(name.size() > 3 ? name.substr(3) : std::string());
    return ShadeRgb(c, (int)(r % (unsigned)(2 * kShadeMax + 1)) - kShadeMax);
}

// A group made here: the palette's colours in turn.
static unsigned NextPaletteTint()
{
    const unsigned t = GroupPalette(g_App.groupPalNext);
    g_App.groupPalNext = (g_App.groupPalNext + 1) % 5;
    return t;
}

// A group is the same group only inside the same folder: two folders can each
// hold a "Phoenix" without the second pair landing in the first one's slots.
static int FindOrCreatePlayer(const std::string& name, const std::string& folder = std::string())
{
    // folder|name -> group, kept across calls: reading a workspace asks once
    // per file, and a scan per ask was quadratic
    static std::unordered_map<std::string, int> keys;
    static unsigned keysGen = 0;
    static int keysFrame = -1;
    static size_t keysN = (size_t)-1;
    if (keysGen != g_PlayerGen || keysFrame != g_FrameNo || keysN != g_App.players.size())
    {
        keys.clear();
        for (int i = 0; i < (int)g_App.players.size(); i++)
            keys.emplace(ToLower(g_App.players[(size_t)i].folder) + "|" + ToLower(g_App.players[(size_t)i].name), i);
    }
    const std::string key = ToLower(folder) + "|" + ToLower(name);
    auto it = keys.find(key);
    if (it != keys.end() && it->second < (int)g_App.players.size()) return it->second;

    Player p;
    p.name = name;
    p.folder = folder;
    if (!folder.empty()) EnsureFolder(folder);
    p.tint  = DefaultGroupTint(name);           // read in from disk; NewGroupIn overrides
    p.tint2 = SlotTint(p.tint, 1);
    g_App.players.push_back(p);
    MarkPlayersChanged();
    keys[key] = (int)g_App.players.size() - 1;
    keysGen = g_PlayerGen;
    keysFrame = g_FrameNo;
    keysN = g_App.players.size();
    return (int)g_App.players.size() - 1;
}

// -----------------------------------------------------------------------------
// Machine code from disk
// -----------------------------------------------------------------------------
static bool ReadBinaryFile(const std::string& path, std::vector<unsigned char>& out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

#include "gui/gui_workspace.inl"   // the workspace folder: reading, writing, watching, undoable delete
#include "gui/gui_export.inl"      // copies of survivors as .asm or binaries, with the warnings

// -----------------------------------------------------------------------------
// Simulation control  --  bridges the workspace model to cw::GuiSession
//
// Play does not run the fight live: it precomputes the whole war on a worker
// thread, then plays the recording back. Everything below therefore talks in
// terms of "cycles" (one survivor clock cycle) and "rounds" (one full move).
// -----------------------------------------------------------------------------
static const float kSpeedBase = 1.75f;
static float RoundsPerSec() { return powf(kSpeedBase, g_App.speedExp); }

// The reference takes the seed as text: Long.parseLong when it parses as a
// number, otherwise java.lang.String.hashCode(). The "SEED!@#=" prefix forces
// the numeric branch, which is how the Java GUI writes a seed back out.
static long long SeedFromText(const char* t)
{
    static const char kPrefix[] = "SEED!@#=";
    const size_t pn = sizeof(kPrefix) - 1;
    if (strncmp(t, kPrefix, pn) == 0) t += pn;
    else
    {
        // Java's parseLong takes an optional sign and nothing else, so anything
        // with stray characters -- spaces included -- falls through to the hash.
        const char* p = t;
        if (*p == '-' || *p == '+') p++;
        bool numeric = (*p != '\0');
        for (; *p; ++p) if (*p < '0' || *p > '9') { numeric = false; break; }
        if (!numeric)
        {
            int h = 0;                       // h = 31*h + c, wrapping at 32 bits
            for (const unsigned char* q = (const unsigned char*)t; *q; ++q)
                h = (int)((unsigned)h * 31u + (unsigned)*q);
            return (long long)h;
        }
    }
    return strtoll(t, NULL, 10);
}

// A group's identity: its folder and its name. Two folders can each hold a
// "Phoenix", so the name alone is not enough -- colours and the inspector find
// a war's groups by this, never by the name.
static std::string PlayerKey(const Player& pl) { return ToLower(pl.folder) + "|" + ToLower(pl.name); }

static int PlayerOfKey(const std::string& key)
{
    if (key.empty()) return -1;
    static std::unordered_map<std::string, int> index;
    static unsigned gen = 0;
    static int frame = -1;
    static size_t n = (size_t)-1;
    if (gen != g_PlayerGen || frame != g_FrameNo || n != g_App.players.size())
    {
        index.clear();
        for (int i = 0; i < (int)g_App.players.size(); i++) index.emplace(PlayerKey(g_App.players[(size_t)i]), i);
        gen = g_PlayerGen; frame = g_FrameNo; n = g_App.players.size();
    }
    auto it = index.find(key);
    return it == index.end() ? -1 : it->second;
}

// The workspace group a group of the recorded war was built from, or -1.
static int WarGroupPlayer(int gi)
{
    if (gi < 0 || gi >= g_App.game.groupCount()) return -1;
    return PlayerOfKey(g_App.game.groupKey(gi));
}

// The warrior of the recorded war that is survivor `slot` of group `p`, or -1.
static int WarriorOfPlayer(int p, int slot)
{
    for (int w = 0; w < g_App.game.numWarriors(); w++)
        if (g_App.game.warriorSlot(w) == slot && !g_App.game.warriorZombie(w) &&
            WarGroupPlayer(g_App.game.warriorGroup(w)) == p)
            return w;
    return -1;
}

// The engine knows a group by its name -- two of the same name would be one
// team -- so where active groups share a name, each fights under its name with
// its folder added ("Phoenix [Old/2015]"), and its survivors with it.
static void SeparateSameNames(std::vector<cw::WarriorGroup>& groups, const std::vector<const Player*>& from)
{
    std::unordered_map<std::string, int> count;
    for (const cw::WarriorGroup& g : groups) count[g.name]++;
    for (size_t i = 0; i < groups.size() && i < from.size(); i++)
    {
        cw::WarriorGroup& g = groups[i];
        if (!from[i] || count[g.name] < 2) continue;
        const std::string old = g.name;
        g.name = old + " [" + (from[i]->folder.empty() ? std::string("top") : from[i]->folder) + "]";
        for (cw::WarriorData& wd : g.warriors)
            if (wd.name.compare(0, old.size(), old) == 0) wd.name = g.name + wd.name.substr(old.size());
    }
}

static std::vector<cw::WarriorGroup> BuildGroups()
{
    std::vector<cw::WarriorGroup> groups;
    std::vector<const Player*> from;
    for (const Player& pl : g_App.players)
    {
        if (!pl.enabled) continue;
        cw::WarriorGroup g;
        g.name = pl.name;
        g.key  = PlayerKey(pl);
        for (int s = 0; s < kSlotCount; s++)
        {
            const Survivor& sv = pl.slots[s];
            if (!sv.runnable()) continue;
            cw::WarriorData wd;
            // "{group}1" / "{group}2" is the survivor's name everywhere -- the
            // timeline, the inspector and the editor all show this and nothing else
            wd.name = pl.name + std::to_string(s + 1);
            wd.code.assign(sv.bin.begin(), sv.bin.end());
            wd.type = (s == 0) ? cw::WarriorType::SURVIVOR_1 : cw::WarriorType::SURVIVOR_2;
            g.warriors.push_back(std::move(wd));
        }
        if (!g.warriors.empty()) { groups.push_back(std::move(g)); from.push_back(&pl); }
    }
    SeparateSameNames(groups, from);

    // All zombies share one group so they never count as separate players.
    cw::WarriorGroup z;
    z.name = "ZoMbIeS";
    for (const Zombie& zb : g_App.zombies)
    {
        if (!zb.runnable()) continue;
        cw::WarriorData wd;
        wd.name = zb.name.empty() ? zb.sv.name : zb.name;
        wd.code.assign(zb.sv.bin.begin(), zb.sv.bin.end());
        wd.type  = zb.heavy ? cw::WarriorType::ZOMBIE_H : cw::WarriorType::ZOMBIE;
        wd.speed = zb.speed;
        z.warriors.push_back(std::move(wd));
    }
    if (!z.warriors.empty()) groups.push_back(std::move(z));
    return groups;
}

// Kick off the silent precompute for the currently enabled groups.
static void GameArm(bool keepSeed = false)
{
    std::vector<cw::WarriorGroup> groups = BuildGroups();
    if (groups.empty())
    {
        g_App.status = !HasWorkspace() ? "Open a workspace first (Ctrl+O)"
                                       : "Make at least one group with an assembled survivor active";
        return;
    }
    g_App.tlLo = 0.0; g_App.tlHi = 0.0;      // timeline re-fits once the replay lands
    // warrior numbers belong to one war; in the next one they are other warriors
    if (g_FollowIp >= 0 && !keepSeed) g_FollowIp = -1;
    g_App.gameSeed = SeedFromText(g_App.seedText);
    // A recompute made for an edit has to be the same war, or the edit means
    // nothing, so the seed only advances for a deliberate new war.
    if (g_App.advanceSeed && !keepSeed)
    {
        // The reference bumps the seed per war; writing it back with the prefix
        // keeps the field numeric so the next bump is exact.
        g_App.gameSeed++;
        snprintf(g_App.seedText, sizeof(g_App.seedText), "SEED!@#=%lld",
                 (long long)g_App.gameSeed);
    }
    g_App.game.arm(std::move(groups), (cw::s64)g_App.gameSeed, g_App.zombieSpeed,
                   g_App.maxRounds, true, g_App.pokes);
    g_App.status = "Precomputing war...";
}

// Edits are staged until the war is asked to move on; then the whole thing is
// played out again with them applied and the view lands one round past the
// change, which is the first round the edit could have affected.
static void SettleEdits()
{
    if (!g_App.pokesPending) return;
    g_App.pokesPending = false;
    g_App.game.seekRoundWhenReady(g_App.pokeRound + 1);
    g_App.status = "Recomputing the war with your change...";
    GameArm(/*keepSeed=*/true);
}

static void GamePlayPause()
{
    if (g_App.game.precomputing()) return;
    if (g_App.pokesPending) { SettleEdits(); return; }
    if (!g_App.game.ready()) { GameArm(); return; }
    // replaying from the end restarts from the top rather than doing nothing
    if (!g_App.game.running() && g_App.game.atEnd() && !g_App.game.reverse) g_App.game.rewind();
    g_App.game.togglePlay();
}

static void GameStepRound(int dir)
{
    if (dir > 0 && g_App.pokesPending) { SettleEdits(); return; }
    if (g_App.game.ready()) g_App.game.stepRound(dir); else GameArm();
}
static void GameContinue()
{
    if (!g_App.game.ready()) { GameArm(); return; }
    g_App.game.continueToEvent(g_App.game.reverse ? -1 : +1);
}

// Reset rewinds the *existing* recording; re-arming (a new war) is explicit.
static void GameReset()
{
    if (g_App.game.ready()) g_App.game.rewind();
    else                    g_App.game.clear();
}

// -----------------------------------------------------------------------------
// Theme
// -----------------------------------------------------------------------------
static void ApplyTheme();
static TextEditor::Palette MakeEditorPalette();

// Everything that has to flip between a light and a dark look asks this rather
// than testing themeIndex, so a hand-edited palette is judged the same way.
static bool ThemeIsDark()
{
    const float r = ((Col::Bg1 >> 16) & 0xFF) / 255.0f;
    const float g = ((Col::Bg1 >>  8) & 0xFF) / 255.0f;
    const float b = ( Col::Bg1        & 0xFF) / 255.0f;
    return (0.2126f * r + 0.7152f * g + 0.0722f * b) < 0.5f;
}

// For marks that have to stand out against whatever is behind them -- the
// timeline playhead and the arena's IP ring both used a hard white, which is
// invisible on a light theme.
static unsigned ContrastInk() { return ThemeIsDark() ? 0xFFFFFF : 0x0B0E13; }

// Copy a preset into the live palette. themeIndex == kThemeCount means the user
// has hand-edited the colours, so nothing is copied over them.
static void SetTheme(int idx)
{
    g_App.themeIndex = idx;
    if (idx >= 0 && idx < kThemeCount)
        for (int i = 0; i < kPalCount; i++) *PalSlot(i) = kThemes[idx].c[i];
    ApplyTheme();
}

static void ApplyTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    // Start from stock every time. ScaleAllSizes() at the end multiplies the
    // style in place, so re-applying on top of an already-scaled style grew
    // every size it touches -- including the ones set nowhere below, like
    // WindowMinSize -- a little more with each theme switch.
    s = ImGuiStyle();
    ImVec4* c = s.Colors;

    s.WindowRounding    = 8.0f;     // floating windows (Options, popped-out inspectors)
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 7.0f;
    s.PopupRounding     = 10.0f;
    s.GrabRounding      = 5.0f;
    s.TabRounding       = 7.0f;
    s.ScrollbarRounding = 8.0f;
    s.MenuItemRounding  = 6.0f;

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.TabBarBorderSize  = 1.0f;
    s.TabBarOverlineSize = 2.0f;

    s.WindowPadding     = ImVec2(14, 12);
    s.FramePadding      = ImVec2(10, 6);
    s.ItemSpacing       = ImVec2(8, 7);
    s.ItemInnerSpacing  = ImVec2(7, 5);
    s.CellPadding       = ImVec2(10, 6);
    s.IndentSpacing     = 18.0f;
    s.ScrollbarSize     = 10.0f;
    s.GrabMinSize       = 12.0f;
    s.ScrollbarPadding  = 2.0f;
    s.SeparatorTextBorderSize = 1.0f;
    s.SeparatorTextPadding    = ImVec2(16, 4);
    s.DockingSeparatorSize    = 2.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.WindowTitleAlign  = ImVec2(0.0f, 0.5f);

    c[ImGuiCol_Text]                  = Hex(Col::Text);
    c[ImGuiCol_TextDisabled]          = Hex(Col::TextDim);
    c[ImGuiCol_WindowBg]              = Hex(Col::Bg1);
    c[ImGuiCol_ChildBg]               = Hex(Col::Bg1);
    c[ImGuiCol_PopupBg]               = Hex(Col::Bg1);   // menus and tooltips: the panel colour, with a border
    c[ImGuiCol_Border]                = Hex(Col::Line);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = Hex(Col::Bg2);
    c[ImGuiCol_FrameBgHovered]        = Hex(Col::Bg3);
    c[ImGuiCol_FrameBgActive]         = Hex(Col::Bg4);
    c[ImGuiCol_TitleBg]               = Hex(Col::Bg0);
    c[ImGuiCol_TitleBgActive]         = Hex(Col::Bg0);
    c[ImGuiCol_TitleBgCollapsed]      = Hex(Col::Bg0);
    c[ImGuiCol_MenuBarBg]             = Hex(Col::Bg0);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]         = Hex(Col::Bg4);
    c[ImGuiCol_ScrollbarGrabHovered]  = Hex(Col::TextDim, 0.55f);
    c[ImGuiCol_ScrollbarGrabActive]   = Hex(Col::Accent, 0.75f);
    c[ImGuiCol_CheckMark]             = Hex(Col::Accent);
    c[ImGuiCol_SliderGrab]            = Hex(Col::Accent, 0.85f);
    c[ImGuiCol_SliderGrabActive]      = Hex(Col::Accent);
    c[ImGuiCol_Button]                = Hex(Col::Bg2);
    c[ImGuiCol_ButtonHovered]         = Hex(Col::Bg3);
    c[ImGuiCol_ButtonActive]          = Hex(Col::Bg4);
    c[ImGuiCol_Header]                = Hex(Col::Accent, 0.22f);
    c[ImGuiCol_HeaderHovered]         = Hex(Col::Accent, 0.12f);
    c[ImGuiCol_HeaderActive]          = Hex(Col::Accent, 0.30f);
    c[ImGuiCol_Separator]             = Hex(Col::Line);
    c[ImGuiCol_SeparatorHovered]      = Hex(Col::Accent, 0.6f);
    c[ImGuiCol_SeparatorActive]       = Hex(Col::Accent);
    c[ImGuiCol_ResizeGrip]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]     = Hex(Col::Accent, 0.4f);
    c[ImGuiCol_ResizeGripActive]      = Hex(Col::Accent, 0.7f);
    // Unselected tabs melt into the bar; the selected one is the same colour
    // as the panel under it, so tab and panel read as one sheet.
    c[ImGuiCol_Tab]                   = Hex(Col::Bg0);
    c[ImGuiCol_TabHovered]            = Hex(Col::Bg3);
    c[ImGuiCol_TabSelected]           = Hex(Col::Bg1);
    c[ImGuiCol_TabSelectedOverline]   = Hex(Col::Accent);
    c[ImGuiCol_TabDimmed]             = Hex(Col::Bg0);
    c[ImGuiCol_TabDimmedSelected]     = Hex(Col::Bg1);
    c[ImGuiCol_TabDimmedSelectedOverline] = Hex(Col::Line);
    c[ImGuiCol_DockingPreview]        = Hex(Col::Accent, 0.35f);
    c[ImGuiCol_DockingEmptyBg]        = Hex(Col::Bg0);
    c[ImGuiCol_PlotLines]             = Hex(Col::Accent);
    c[ImGuiCol_PlotHistogram]         = Hex(Col::Accent2);
    c[ImGuiCol_TableHeaderBg]         = Hex(Col::Bg2);
    c[ImGuiCol_TableBorderStrong]     = Hex(Col::Line);
    c[ImGuiCol_TableBorderLight]      = Hex(Col::Line, 0.6f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = Hex(Col::Text, 0.035f);
    c[ImGuiCol_TextSelectedBg]        = Hex(Col::Accent, 0.30f);
    c[ImGuiCol_NavCursor]             = Hex(Col::Accent, 0.7f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.55f);

    if (g_Scale != 1.0f)
        s.ScaleAllSizes(g_Scale);

    g_App.editor.SetPalette(MakeEditorPalette());
}

static TextEditor::Palette MakeEditorPalette()
{
    const bool dark = ThemeIsDark();
    TextEditor::Palette p = dark ? TextEditor::GetDarkPalette() : TextEditor::GetLightPalette();
    // Syntax colours are picked per background so the editor tracks the theme
    // instead of staying dark under a light one.
    const unsigned kw   = dark ? 0x7AA7FF : 0x1D4ED8;
    const unsigned num  = dark ? 0xE3B341 : 0x9A6700;
    const unsigned str  = dark ? 0x7EE0A8 : 0x0F7B4F;
    const unsigned punc = dark ? 0x9AA5B4 : 0x57606A;
    const unsigned pre  = dark ? 0xC77DFF : 0x8250DF;
    const unsigned idn  = dark ? 0xCBD3DE : 0x24292F;
    const unsigned com  = dark ? 0x5C6675 : 0x8C959F;
    const unsigned line = dark ? 0x4E586A : 0xA0A8B4;
    const unsigned wash = dark ? 0xFFFFFF : 0x000000;

    p[(int)TextEditor::PaletteIndex::Default]                 = HexU(Col::Text);
    p[(int)TextEditor::PaletteIndex::Keyword]                 = HexU(kw);
    p[(int)TextEditor::PaletteIndex::Number]                  = HexU(num);
    p[(int)TextEditor::PaletteIndex::String]                  = HexU(str);
    p[(int)TextEditor::PaletteIndex::CharLiteral]             = HexU(str);
    p[(int)TextEditor::PaletteIndex::Punctuation]             = HexU(punc);
    p[(int)TextEditor::PaletteIndex::Preprocessor]            = HexU(pre);
    p[(int)TextEditor::PaletteIndex::Identifier]              = HexU(idn);
    p[(int)TextEditor::PaletteIndex::KnownIdentifier]         = HexU(Col::Accent2);
    p[(int)TextEditor::PaletteIndex::PreprocIdentifier]       = HexU(pre);
    p[(int)TextEditor::PaletteIndex::Comment]                 = HexU(com);
    p[(int)TextEditor::PaletteIndex::MultiLineComment]        = HexU(com);
    p[(int)TextEditor::PaletteIndex::Background]              = HexU(Col::Bg2);
    // the caret has to stand against the editor background, which on a light
    // theme the accent blue barely does
    p[(int)TextEditor::PaletteIndex::Cursor]                  = HexU(Col::Text);
    p[(int)TextEditor::PaletteIndex::Selection]               = HexU(Col::Accent, 0.30f);
    p[(int)TextEditor::PaletteIndex::ErrorMarker]             = HexU(Col::Danger, 0.55f);
    p[(int)TextEditor::PaletteIndex::Breakpoint]              = HexU(Col::Danger, 0.75f);
    p[(int)TextEditor::PaletteIndex::LineNumber]              = HexU(line);
    p[(int)TextEditor::PaletteIndex::CurrentLineFill]         = HexU(wash, 0.045f);
    p[(int)TextEditor::PaletteIndex::CurrentLineFillInactive] = HexU(wash, 0.025f);
    p[(int)TextEditor::PaletteIndex::CurrentLineEdge]         = HexU(wash, 0.06f);
    return p;
}

// -----------------------------------------------------------------------------
// Small drawing helpers
// -----------------------------------------------------------------------------
struct BtnState
{
    bool   clicked = false;
    bool   hovered = false;
    bool   held    = false;
    ImVec2 mn, mx;
};

static BtnState BareButton(const char* id, const ImVec2& size)
{
    BtnState st;
    st.mn = ImGui::GetCursorScreenPos();
    st.clicked = ImGui::InvisibleButton(id, size);
    st.hovered = ImGui::IsItemHovered();
    st.held    = ImGui::IsItemActive();
    st.mx      = ImVec2(st.mn.x + size.x, st.mn.y + size.y);
    return st;
}

static void DrawGlyphMinimize(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x + s, c.y), col, 1.4f);
}

static void DrawGlyphMaximize(ImDrawList* dl, const ImVec2& c, float s, ImU32 col, bool restore)
{
    if (restore)
    {
        dl->AddRect(ImVec2(c.x - s, c.y - s + 2.5f), ImVec2(c.x + s - 2.5f, c.y + s), col, 1.5f, 0, 1.3f);
        dl->AddLine(ImVec2(c.x - s + 2.5f, c.y - s + 1.0f), ImVec2(c.x + s, c.y - s + 1.0f), col, 1.3f);
        dl->AddLine(ImVec2(c.x + s, c.y - s + 1.0f), ImVec2(c.x + s, c.y + s - 2.5f), col, 1.3f);
    }
    else
    {
        dl->AddRect(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), col, 1.5f, 0, 1.3f);
    }
}

static void DrawGlyphClose(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), col, 1.5f);
    dl->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x - s, c.y + s), col, 1.5f);
}

static void DrawGlyphPlay(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddTriangleFilled(ImVec2(c.x - s * 0.7f, c.y - s), ImVec2(c.x - s * 0.7f, c.y + s), ImVec2(c.x + s, c.y), col);
}

static void DrawGlyphPause(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddRectFilled(ImVec2(c.x - s * 0.75f, c.y - s), ImVec2(c.x - s * 0.15f, c.y + s), col, 1.0f);
    dl->AddRectFilled(ImVec2(c.x + s * 0.15f, c.y - s), ImVec2(c.x + s * 0.75f, c.y + s), col, 1.0f);
}

static void DrawGlyphStep(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddTriangleFilled(ImVec2(c.x - s, c.y - s), ImVec2(c.x - s, c.y + s), ImVec2(c.x + s * 0.25f, c.y), col);
    dl->AddRectFilled(ImVec2(c.x + s * 0.45f, c.y - s), ImVec2(c.x + s * 0.9f, c.y + s), col, 1.0f);
}

static void DrawGlyphStop(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddRectFilled(ImVec2(c.x - s * 0.85f, c.y - s * 0.85f), ImVec2(c.x + s * 0.85f, c.y + s * 0.85f), col, 2.0f);
}

// step backwards one cycle: bar then a left-pointing triangle
static void DrawGlyphStepBack(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddRectFilled(ImVec2(c.x - s * 0.9f, c.y - s), ImVec2(c.x - s * 0.45f, c.y + s), col, 1.0f);
    dl->AddTriangleFilled(ImVec2(c.x + s, c.y - s), ImVec2(c.x + s, c.y + s), ImVec2(c.x - s * 0.25f, c.y), col);
}

// one full move backwards / forwards: a pair of triangles
static void DrawGlyphRewind(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddTriangleFilled(ImVec2(c.x + s * 0.1f, c.y - s), ImVec2(c.x + s * 0.1f, c.y + s), ImVec2(c.x - s, c.y), col);
    dl->AddTriangleFilled(ImVec2(c.x + s,        c.y - s), ImVec2(c.x + s,        c.y + s), ImVec2(c.x - s * 0.1f, c.y), col);
}
static void DrawGlyphFastFwd(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddTriangleFilled(ImVec2(c.x - s,        c.y - s), ImVec2(c.x - s,        c.y + s), ImVec2(c.x + s * 0.1f, c.y), col);
    dl->AddTriangleFilled(ImVec2(c.x - s * 0.1f, c.y - s), ImVec2(c.x - s * 0.1f, c.y + s), ImVec2(c.x + s,        c.y), col);
}

// continue to the next event: triangle into a wall
static void DrawGlyphContinue(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddTriangleFilled(ImVec2(c.x - s, c.y - s), ImVec2(c.x - s, c.y + s), ImVec2(c.x + s * 0.35f, c.y), col);
    dl->AddRectFilled(ImVec2(c.x + s * 0.55f, c.y - s), ImVec2(c.x + s, c.y + s), col, 1.0f);
}

static void DrawGlyphPlus(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x + s, c.y), col, 1.6f);
    dl->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y + s), col, 1.6f);
}

static void DrawGlyphGear(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    const int teeth = 8;
    for (int i = 0; i < teeth; i++)
    {
        const float a = (float)i * 3.14159265f * 2.0f / (float)teeth;
        const ImVec2 d(cosf(a), sinf(a));
        dl->AddLine(ImVec2(c.x + d.x * s * 0.62f, c.y + d.y * s * 0.62f),
                    ImVec2(c.x + d.x * s * 1.05f, c.y + d.y * s * 1.05f), col, 2.0f * g_Scale);
    }
    dl->AddCircle(c, s * 0.66f, col, 16, 1.8f * g_Scale);
    dl->AddCircleFilled(c, s * 0.24f, col, 10);
}

// A page with a folded corner: one survivor file, as opposed to a whole folder.
static void DrawGlyphSurvivor(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    const float w = s * 0.78f, h = s * 1.05f, fold = s * 0.42f;
    const ImVec2 tl(c.x - w, c.y - h), br(c.x + w, c.y + h);
    dl->PathLineTo(tl);
    dl->PathLineTo(ImVec2(br.x - fold, tl.y));
    dl->PathLineTo(ImVec2(br.x, tl.y + fold));
    dl->PathLineTo(br);
    dl->PathLineTo(ImVec2(tl.x, br.y));
    dl->PathStroke(col, ImDrawFlags_Closed, 1.6f * g_Scale);
    dl->AddLine(ImVec2(br.x - fold, tl.y), ImVec2(br.x - fold, tl.y + fold), col, 1.4f * g_Scale);
    dl->AddLine(ImVec2(br.x - fold, tl.y + fold), ImVec2(br.x, tl.y + fold), col, 1.4f * g_Scale);
}

static void DrawGlyphFolder(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    dl->AddRect(ImVec2(c.x - s, c.y - s * 0.6f), ImVec2(c.x + s, c.y + s * 0.8f), col, 1.5f, 0, 1.3f);
    dl->AddLine(ImVec2(c.x - s, c.y - s * 0.6f), ImVec2(c.x - s * 0.2f, c.y - s * 0.6f), col, 1.3f);
    dl->AddLine(ImVec2(c.x - s * 0.2f, c.y - s * 0.6f), ImVec2(c.x, c.y - s), col, 1.3f);
}

// Text turned a quarter-turn anticlockwise, reading bottom to top. `pos` is
// where the run starts -- its bottom-left corner once turned -- and it takes up
// one line height to the right of that and its own length upwards.
//
// ImGui has no rotated text, so the run is laid out flat at `pos` and its
// vertices are then swung round that point. It goes through the font rather
// than AddText because AddText throws away glyphs that fall outside the clip
// rectangle *where they would have sat before the turn*; the real clipping is
// the draw command's scissor, which is applied after it.
static void AddTextUp(ImDrawList* dl, const ImVec2& pos, ImU32 col, const char* text)
{
    if (!text || !text[0] || (col & IM_COL32_A_MASK) == 0) return;
    ImGui::CalcTextSize(text);                       // makes sure every glyph is baked first
    const ImVec2 o(ImTrunc(pos.x), ImTrunc(pos.y));
    const int first = dl->VtxBuffer.Size;
    ImGui::GetFont()->RenderText(dl, ImGui::GetFontSize(), o, col,
                                 ImVec4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX), text, NULL);
    for (int i = first; i < dl->VtxBuffer.Size; i++)
    {
        ImVec2& v = dl->VtxBuffer[i].pos;
        const float dx = v.x - o.x, dy = v.y - o.y;
        v = ImVec2(o.x + dy, o.y - dx);
    }
}

// A pill-shaped read-only value chip: "LABEL  value"
static void Chip(const char* label, const char* value, unsigned tint)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiStyle& st = ImGui::GetStyle();
    const float padX = 9.0f * g_Scale;
    const float h    = 22.0f * g_Scale;

    const ImVec2 lsz = ImGui::CalcTextSize(label);
    const ImVec2 vsz = ImGui::CalcTextSize(value);
    const float  w   = padX * 2 + lsz.x + 7 * g_Scale + vsz.x;

    ImVec2 p = ImGui::GetCursorScreenPos();
    p.y += ImMax(0.0f, (ImGui::GetFrameHeight() - h) * 0.5f);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), HexU(Col::Bg2), h * 0.5f);
    dl->AddText(ImVec2(p.x + padX, p.y + (h - lsz.y) * 0.5f), HexU(Col::TextDim), label);
    dl->AddText(ImVec2(p.x + padX + lsz.x + 7 * g_Scale, p.y + (h - vsz.y) * 0.5f), HexU(tint), value);

    ImGui::Dummy(ImVec2(w, ImGui::GetFrameHeight()));
    (void)st;
}

// -----------------------------------------------------------------------------
// Long-press bookkeeping (hold a row / a survivor button to disable it)
// -----------------------------------------------------------------------------
// Nothing is drawn for the first kLongPressDelay of a hold, so an ordinary
// click to open or select a row never flashes the disable bar; the remaining
// stretch is what the bar fills over.
static const float kLongPressDelay = 0.20f;
static const float kLongPressTime  = 0.62f;
static bool    g_LongPressFired = false;   // the current click was already consumed
static ImGuiID g_LongPressId = 0;
static float   g_LongPressProgress = 0.0f;
static int     g_OptionsTab = 0;
static bool    g_ResetLayout = false;
// Set by the font pickers; the swap happens between frames, never inside one.
static bool    g_FontsDirty = false;
static int     g_FocusEditorFrames = 0;

static void BeginLongPressFrame()
{
    // Keep the flag alive during the release frame so the click that ends a long
    // press does not also change the selection.
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        g_LongPressFired = false;
        g_LongPressId = 0;
        g_LongPressProgress = 0.0f;
    }
}

// Call right after submitting an item. Returns true once, when the hold completes.
static bool CheckLongPress(bool& clickedInOut)
{
    bool fired = false;
    // a row being dragged to a folder is held just as long, but is not a hold
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsDragDropActive())
    {
        const float t = ImGui::GetIO().MouseDownDuration[ImGuiMouseButton_Left];
        g_LongPressId = ImGui::GetItemID();
        g_LongPressProgress = ImClamp((t - kLongPressDelay) / (kLongPressTime - kLongPressDelay),
                                      0.0f, 1.0f);
        if (!g_LongPressFired && t >= kLongPressTime)
        {
            g_LongPressFired = true;
            fired = true;
        }
    }
    if (g_LongPressFired) clickedInOut = false;   // swallow the click
    return fired;
}

static void DrawLongPressHint(const ImVec2& mn, const ImVec2& mx)
{
    if (g_LongPressId != ImGui::GetItemID() || g_LongPressProgress <= 0.02f) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = 2.0f * g_Scale;
    dl->AddRectFilled(ImVec2(mn.x, mx.y - h), ImVec2(mn.x + (mx.x - mn.x) * g_LongPressProgress, mx.y),
                      HexU(Col::Warn, 0.9f), h * 0.5f);
}

// -----------------------------------------------------------------------------
// Menus
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Title bar (rendered as the root window's menu bar so real menus work)
// -----------------------------------------------------------------------------
static void DrawTitleBar()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    const ImVec2 barMin(win->Pos.x, win->Pos.y);
    const float  barW = win->Size.x;

    dl->AddLine(ImVec2(barMin.x, barMin.y + g_TitleBarHeight - 1.0f),
                ImVec2(barMin.x + barW, barMin.y + g_TitleBarHeight - 1.0f), HexU(Col::Line));

    // --- logo mark ---------------------------------------------------------
    const float pad = 12.0f * g_Scale;
    const float s   = 8.0f * g_Scale;
    const ImVec2 c(barMin.x + pad + s, barMin.y + g_TitleBarHeight * 0.5f);
    dl->AddRectFilled(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), HexU(Col::Accent), 5.0f * g_Scale);
    dl->AddRectFilled(ImVec2(c.x - s * 0.34f, c.y - s * 0.34f), ImVec2(c.x + s * 0.34f, c.y + s * 0.34f),
                      HexU(Col::Bg0), 2.0f * g_Scale);

    const char* name = "CWEST";
    const char* sub  = "8086";
    const ImVec2 nsz = ImGui::CalcTextSize(name);
    const ImVec2 ssz = ImGui::CalcTextSize(sub);
    float x = c.x + s + 10 * g_Scale;
    dl->AddText(ImVec2(x, barMin.y + (g_TitleBarHeight - nsz.y) * 0.5f), HexU(Col::Text), name);
    x += nsz.x + 7 * g_Scale;
    dl->AddText(ImVec2(x, barMin.y + (g_TitleBarHeight - ssz.y) * 0.5f), HexU(Col::TextDim), sub);
    x += ssz.x + 16 * g_Scale;

    // the open workspace, the way an editor names its folder
    if (HasWorkspace())
    {
        const std::string ws = FileNameOf(WsRoot());
        dl->AddLine(ImVec2(x - 8 * g_Scale, barMin.y + g_TitleBarHeight * 0.3f),
                    ImVec2(x - 8 * g_Scale, barMin.y + g_TitleBarHeight * 0.7f), HexU(Col::Line));
        const ImVec2 wsz = ImGui::CalcTextSize(ws.c_str());
        dl->AddText(ImVec2(x, barMin.y + (g_TitleBarHeight - wsz.y) * 0.5f), HexU(Col::TextDim), ws.c_str());
        x += wsz.x;
    }

    // The menu bar is gone -- everything it held is a shortcut, a control on the
    // panel it belongs to, or a row in the options window -- so the middle of
    // the strip is left as drag surface.
    (void)x;

    // --- window controls ---------------------------------------------------
    const float bw = 44.0f * g_Scale;
    const float bh = g_TitleBarHeight - 1.0f;
    const float bs = 24.0f * g_Scale;
    const float gx = barMin.x + barW - bw * 3.0f - bs - 10.0f * g_Scale;
    AddNoDragRect(ImVec2(gx - 4 * g_Scale, barMin.y), ImVec2(barMin.x + barW, barMin.y + g_TitleBarHeight));

    // options sits just left of the window buttons, on the same baseline
    ImGui::SetCursorScreenPos(ImVec2(gx, barMin.y + (g_TitleBarHeight - bs) * 0.5f));
    BtnState b = BareButton("##opts", ImVec2(bs, bs));
    if (b.hovered || g_App.showOptions)
        dl->AddRectFilled(b.mn, b.mx, HexU(b.held ? Col::Bg4 : Col::Bg3), 6 * g_Scale);
    DrawGlyphGear(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 6 * g_Scale,
                  HexU((b.hovered || g_App.showOptions) ? Col::Accent : Col::TextDim));
    if (b.hovered) ImGui::SetTooltip("Options (Ctrl+,)");
    if (b.clicked) g_App.showOptions = !g_App.showOptions;

    const float x0 = barMin.x + barW - bw * 3.0f;
    ImGui::SetCursorScreenPos(ImVec2(x0, barMin.y));
    b = BareButton("##min", ImVec2(bw, bh));
    if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3));
    DrawGlyphMinimize(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f + 1), 5 * g_Scale, HexU(Col::Text));
    if (b.clicked) ShowWindow(g_Hwnd, SW_MINIMIZE);

    ImGui::SetCursorScreenPos(ImVec2(x0 + bw, barMin.y));
    b = BareButton("##max", ImVec2(bw, bh));
    if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3));
    DrawGlyphMaximize(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 5 * g_Scale,
                      HexU(Col::Text), IsWindowMaximized(g_Hwnd));
    if (b.clicked) ShowWindow(g_Hwnd, IsWindowMaximized(g_Hwnd) ? SW_RESTORE : SW_MAXIMIZE);

    ImGui::SetCursorScreenPos(ImVec2(x0 + bw * 2, barMin.y));
    b = BareButton("##close", ImVec2(bw, bh));
    if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Danger));
    DrawGlyphClose(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 5 * g_Scale,
                   b.hovered ? HexU(0xFFFFFF) : HexU(Col::Text));
    if (b.clicked) glfwSetWindowShouldClose(g_Window, GLFW_TRUE);

    // While a popup is open the whole strip must behave like client area so the
    // popup can be dismissed by clicking anywhere in it.
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        AddNoDragRect(barMin, ImVec2(barMin.x + barW, barMin.y + g_TitleBarHeight));

    // Dragging a floating panel across the strip used to hand the gesture to
    // the OS mid-drag, which moved the whole app instead. While ImGui owns a
    // drag the strip is client area, so the panel keeps the mouse.
    if (ImGui::GetCurrentContext()->MovingWindow != NULL || ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
        AddNoDragRect(barMin, ImVec2(barMin.x + barW, barMin.y + g_TitleBarHeight));
}

// -----------------------------------------------------------------------------
// Toolbar
// -----------------------------------------------------------------------------
// Two arrows chasing round a circle: "play it all again".
static void DrawGlyphRecompute(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    const float r = s * 0.85f, th = 1.6f * g_Scale;
    dl->PathArcTo(c, r, -2.6f, 0.35f, 14);
    dl->PathStroke(col, 0, th);
    dl->PathArcTo(c, r, 0.55f, 3.5f, 14);
    dl->PathStroke(col, 0, th);
    // arrow heads at the end of each arc, pointing along it
    auto head = [&](float a) {
        const ImVec2 p(c.x + cosf(a) * r, c.y + sinf(a) * r);
        const ImVec2 t(-sinf(a), cosf(a));            // tangent, direction of travel
        const ImVec2 n(cosf(a), sinf(a));
        const float k = s * 0.45f;
        dl->AddTriangleFilled(ImVec2(p.x + t.x * k, p.y + t.y * k),
                              ImVec2(p.x - n.x * k * 0.8f, p.y - n.y * k * 0.8f),
                              ImVec2(p.x + n.x * k * 0.8f, p.y + n.y * k * 0.8f), col);
    };
    head(0.35f);
    head(3.5f);
}

// A crosshair: "find it on the arena".
static void DrawGlyphTarget(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    const float th = 1.4f * g_Scale;
    dl->AddCircle(c, s * 0.62f, col, 16, th);
    dl->AddCircleFilled(c, s * 0.18f, col, 8);
    dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - s * 0.42f, c.y), col, th);
    dl->AddLine(ImVec2(c.x + s * 0.42f, c.y), ImVec2(c.x + s, c.y), col, th);
    dl->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y - s * 0.42f), col, th);
    dl->AddLine(ImVec2(c.x, c.y + s * 0.42f), ImVec2(c.x, c.y + s), col, th);
}

// A dot held in four corner brackets: "locked on -- keep following it".
static void DrawGlyphFollow(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    const float th = 1.4f * g_Scale;
    dl->AddCircleFilled(c, s * 0.26f, col, 10);
    const float a = s * 0.9f, k = s * 0.42f;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
        {
            const ImVec2 corner(c.x + sx * a, c.y + sy * a);
            dl->AddLine(corner, ImVec2(corner.x - sx * k, corner.y), col, th);
            dl->AddLine(corner, ImVec2(corner.x, corner.y - sy * k), col, th);
        }
}

// A box with an arrow leaving its corner: "open in its own window".
static void DrawGlyphPopOut(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    const float th = 1.4f * g_Scale;
    const ImVec2 mn(c.x - s * 0.85f, c.y - s * 0.55f), mx(c.x + s * 0.55f, c.y + s * 0.85f);
    dl->PathLineTo(ImVec2(mn.x + s * 0.55f, mn.y));
    dl->PathLineTo(mn);
    dl->PathLineTo(ImVec2(mn.x, mx.y));
    dl->PathLineTo(mx);
    dl->PathLineTo(ImVec2(mx.x, mx.y - s * 0.55f));
    dl->PathStroke(col, 0, th);
    const ImVec2 tip(c.x + s, c.y - s);
    dl->AddLine(ImVec2(c.x - s * 0.15f, c.y + s * 0.15f), tip, col, th);
    dl->AddLine(tip, ImVec2(tip.x - s * 0.6f, tip.y), col, th);
    dl->AddLine(tip, ImVec2(tip.x, tip.y + s * 0.6f), col, th);
}

// Buttons that run edge to edge -- a "train" -- round only their outer corners:
// the first its left pair, the last its right pair, the middle ones none.
static ImDrawFlags TrainCorners(int i, int n)
{
    if (n <= 1)     return ImDrawFlags_RoundCornersAll;
    if (i == 0)     return ImDrawFlags_RoundCornersLeft;
    if (i == n - 1) return ImDrawFlags_RoundCornersRight;
    return ImDrawFlags_RoundCornersNone;
}

// The hairline between two carriages of a train, drawn on the left edge of
// every carriage but the first.
static void TrainJoint(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx, int i)
{
    if (i <= 0) return;
    const float inset = (mx.y - mn.y) * 0.22f;
    dl->AddLine(ImVec2(mn.x, mn.y + inset), ImVec2(mn.x, mx.y - inset), HexU(Col::Line), 1.0f);
}

static const float kBarH = 28.0f;   // height of every control on a panel's toolbar

// An ordinary ImGui field (drag, input, combo) as one carriage of a train:
// the carriage is drawn here with the corners its place calls for, and the
// widget's own frame is switched off so it sits inside it. Pair every Begin
// with an End right after the widget.
struct TrainField { ImVec2 mn, mx; ImDrawFlags corners; };
static TrainField TrainFieldBegin(float w, int i, int n)
{
    TrainField t;
    t.mn = ImGui::GetCursorScreenPos();
    t.mx = ImVec2(t.mn.x + w, t.mn.y + kBarH * g_Scale);
    t.corners = TrainCorners(i, n);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool hot = ImGui::IsMouseHoveringRect(t.mn, t.mx) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    dl->AddRectFilled(t.mn, t.mx, HexU(hot ? Col::Bg3 : Col::Bg2, ImGui::GetStyle().Alpha), 7.0f * g_Scale, t.corners);
    TrainJoint(dl, t.mn, t.mx, i);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(10 * g_Scale, (kBarH * g_Scale - ImGui::GetFontSize()) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    const ImVec4 clear(0, 0, 0, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, clear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, clear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, clear);
    ImGui::PushStyleColor(ImGuiCol_Button, clear);           // a combo's arrow square
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, clear);
    ImGui::SetNextItemWidth(w);
    return t;
}
static void TrainFieldEnd(const TrainField& t)
{
    const bool active = ImGui::IsItemActive();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    if (active)
        ImGui::GetWindowDrawList()->AddRect(t.mn, t.mx, HexU(Col::Accent, 0.7f), 7.0f * g_Scale,
                                            t.corners, 1.2f);
}

static bool ToolButton(const char* id, int glyph, const char* tip, bool accent = false, bool enabled = true,
                       ImDrawFlags corners = ImDrawFlags_RoundCornersAll, int trainIndex = 0)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = kBarH * g_Scale;
    BtnState b = BareButton(id, ImVec2(h, h));

    ImU32 bg = accent ? HexU(Col::Accent, b.held ? 1.0f : (b.hovered ? 0.95f : 0.85f))
                      : HexU(b.held ? Col::Bg4 : (b.hovered ? Col::Bg3 : Col::Bg2));
    if (!enabled) bg = HexU(Col::Bg2, 0.5f);
    dl->AddRectFilled(b.mn, b.mx, bg, 7.0f * g_Scale, corners);
    if (!accent) TrainJoint(dl, b.mn, b.mx, trainIndex);

    const ImU32 fg = accent ? HexU(Col::Bg0) : HexU(enabled ? Col::Text : Col::TextDim);
    const ImVec2 c((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f);
    const float s = 6.0f * g_Scale;
    switch (glyph)
    {
        case 0: DrawGlyphPlay(dl, c, s, fg);     break;
        case 1: DrawGlyphPause(dl, c, s, fg);    break;
        case 2: DrawGlyphStep(dl, c, s, fg);     break;
        case 3: DrawGlyphStop(dl, c, s, fg);     break;
        case 4: DrawGlyphPlus(dl, c, s, fg);     break;
        case 5: DrawGlyphFolder(dl, c, s, fg);   break;
        case 6: DrawGlyphStepBack(dl, c, s, fg); break;
        case 7: DrawGlyphRewind(dl, c, s, fg);   break;
        case 8: DrawGlyphFastFwd(dl, c, s, fg);  break;
        case 9: DrawGlyphContinue(dl, c, s, fg); break;
        case 10: DrawGlyphRecompute(dl, c, s, fg); break;
        case 11: DrawGlyphTarget(dl, c, s, fg);  break;
        case 12: DrawGlyphPopOut(dl, c, s * 0.9f, fg); break;
        case 13: DrawGlyphFollow(dl, c, s, fg);  break;
        default: break;
    }
    if (b.hovered && tip) ImGui::SetTooltip("%s", tip);
    return b.clicked && enabled;
}

// -----------------------------------------------------------------------------
// Workspace panel  (drawn into the main window, not a docked window)
// -----------------------------------------------------------------------------
// A collapsed group line is exactly two survivor chips wide, and nothing more.
static const float kCollapsedChip = 22.0f;
static float CollapsedGroupsWidth()
{
    return kCollapsedChip * 2.0f * g_Scale + 4.0f * g_Scale + 22.0f * g_Scale;
}

// Where files dropped on the window would land, by what is under the mouse:
// rebuilt every frame as the rows are drawn, read by the drop callback.
struct DropZone { ImVec2 mn, mx; std::string folder; bool zombies; };
static std::vector<DropZone> g_DropZones, g_DropZonesBuild;
static void AddDropZone(const ImVec2& mn, const ImVec2& mx, const std::string& folder, bool zombies)
{
    DropZone z; z.mn = mn; z.mx = mx; z.folder = folder; z.zombies = zombies;
    g_DropZonesBuild.push_back(z);
}

// What a menu item or a drag on one row acts on: the whole selection when the
// row is part of it, otherwise just the row.
static std::vector<RefKey> ItemsForAction(const Ref& row)
{
    std::vector<RefKey> out;
    if (IsSelected(row) && g_App.selection.size() > 1)
        for (const Ref& r : g_App.selection)
        {
            RefKey k = KeyOfRef(r);
            if (k.kind == 0 && k.slot >= 0) continue;          // survivors travel with their group
            if (k.kind >= 0) out.push_back(k);
        }
    if (out.empty()) { RefKey k = KeyOfRef(row); if (k.kind == 0) k.slot = -1; out.push_back(k); }
    return out;
}

static bool MultiSelected(const Ref& row)
{
    return IsSelected(row) && g_App.selection.size() > 1;
}

// The folders a drag or "Move to folder" may put these into.
static bool CanMoveInto(const std::vector<RefKey>& items, const std::string& target)
{
    for (const RefKey& k : items)
        if (k.kind == 2 && FolderUnder(target, k.folder)) return false;   // not into itself
    return true;
}

static void QueueMove(const std::vector<RefKey>& items, const std::string& folder)
{
    if (!CanMoveInto(items, folder)) { g_App.status = "A folder can't go inside itself"; return; }
    WsOp op;
    op.kind = WsOp::MoveGroups;
    op.items = items;
    op.folder = folder;
    QueueWsOp(op);
}

static void QueueDelete(const std::vector<RefKey>& items)
{
    WsOp op;
    op.kind = WsOp::Delete;
    op.items = items;
    QueueWsOp(op);
}

// "Move to folder": every folder, as the tree, plus a new one.
static void MoveToFolderMenu(const std::vector<RefKey>& items, const std::string& current)
{
    if (!ImGui::BeginMenu("Move to folder")) return;
    if (items.size() > 1) ImGui::TextDisabled("%d items", (int)items.size());
    const bool mixed = (current == "\x01");               // a selection from several places
    const std::string here = mixed ? std::string() : current;
    if (ImGui::MenuItem("(top)", NULL, !mixed && current.empty())) QueueMove(items, std::string());
    std::vector<int> order;
    for (int i = 0; i < (int)g_App.folders.size(); i++) order.push_back(i);   // already sorted by path
    for (int fi : order)
    {
        const std::string& p = g_App.folders[fi].path;
        const bool ok = CanMoveInto(items, p);
        std::string label = std::string((size_t)(FolderDepth(p) - 1) * 3, ' ') + FolderLeaf(p);
        if (ImGui::MenuItem((label + "##mv" + p).c_str(), NULL, ToLower(p) == ToLower(current), ok))
            QueueMove(items, p);
    }
    ImGui::Separator();
    static char newFolder[64] = "";
    ImGui::SetNextItemWidth(170 * g_Scale);
    if (ImGui::InputTextWithHint("##newfolder", "New folder, Enter...", newFolder, sizeof(newFolder),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
    {
        const std::string name = CleanFolderName(newFolder);
        if (!name.empty()) QueueMove(items, FolderJoin(here, name));
        else               g_App.status = "That can't be a folder name";
        newFolder[0] = 0;
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Made inside the folder these are in now");
    ImGui::EndMenu();
}

// "Run against folders": the groups each fight every picked folder's groups,
// one folder at a time -- never two folders in the same battle -- and the
// board keeps a page of scores per folder.
//
// The folders are picked in a window of their own (a menu of every folder grew
// as wide as the longest path and as tall as the workspace): a tree that opens
// only what you open, a filter box, and a size of its own you can drag.
static std::set<std::string> g_RunPick;    // lower-case paths
struct RunFolderPicker
{
    bool                open = false;
    bool                appearing = false;
    std::vector<RefKey> groups;               // who plays
    char                filter[96] = "";
    int                 openAll = 0;          // 1 expand every node next frame, -1 collapse
};
static RunFolderPicker g_RunPicker;

static void RunAgainstFoldersMenu(const std::vector<RefKey>& items)
{
    std::vector<RefKey> groups;
    for (const RefKey& k : items) if (k.kind == 0) groups.push_back(k);
    if (groups.empty()) return;
    const bool any = !g_App.folders.empty();
    if (ImGui::MenuItem("Run against folders...", NULL, false, any))
    {
        g_RunPicker.open = true;
        g_RunPicker.appearing = true;
        g_RunPicker.groups = groups;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(any ? "Pick folders: each one is a tournament of its own for these groups"
                              : "No folders yet");
}

// Picks (or drops) every folder nested inside `path` -- not `path` itself,
// which would hold them all again.
static void RunPickNested(const std::string& path)
{
    const std::string key = ToLower(path);
    std::vector<std::string> nested;
    for (const GroupFolder& g : g_App.folders)
        if (ToLower(g.path) != key && FolderUnder(g.path, path)) nested.push_back(ToLower(g.path));
    bool allOn = !nested.empty();
    for (const std::string& n : nested) if (!g_RunPick.count(n)) allOn = false;
    for (const std::string& n : nested) { if (allOn) g_RunPick.erase(n); else g_RunPick.insert(n); }
    g_RunPick.erase(key);
    if (nested.empty()) g_App.status = FolderLeaf(path) + " has no folders inside it";
}

struct RunPickCounts
{
    std::map<std::string, int> groups;     // lower path -> groups in it and below
    std::map<std::string, int> picked;     // lower path -> picked folders strictly below
    std::set<std::string>      shown;      // with a filter: folders that match, and their parents
};

// One level of the picker's tree.
static void RunPickTree(const std::string& parent, const RunPickCounts& c, bool filtering)
{
    for (int fi : FolderChildren(parent))
    {
        if (fi >= (int)g_App.folders.size()) continue;
        const std::string path = g_App.folders[fi].path;
        const std::string key = ToLower(path);
        if (filtering && !c.shown.count(key)) continue;
        auto gi = c.groups.find(key);
        auto pi = c.picked.find(key);
        const int nGroups = gi == c.groups.end() ? 0 : gi->second;
        const int nPicked = pi == c.picked.end() ? 0 : pi->second;
        const bool leaf = FolderChildren(path).empty();

        ImGui::PushID(path.c_str());
        bool on = g_RunPick.count(key) > 0;
        if (ImGui::Checkbox("##pick", &on)) { if (on) g_RunPick.insert(key); else g_RunPick.erase(key); }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%d group%s, the folders inside it included", path.c_str(), nGroups, nGroups == 1 ? "" : "s");
        ImGui::SameLine(0, 4 * g_Scale);
        if (g_RunPicker.openAll != 0 && !leaf) ImGui::SetNextItemOpen(g_RunPicker.openAll > 0);
        else if (filtering && !leaf) ImGui::SetNextItemOpen(true);
        char label[200];
        if (nPicked > 0)
            snprintf(label, sizeof(label), "%s  (%d)   %d inside picked", FolderLeaf(path).c_str(), nGroups, nPicked);
        else
            snprintf(label, sizeof(label), "%s  (%d)", FolderLeaf(path).c_str(), nGroups);
        const bool open = ImGui::TreeNodeEx("##node", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                                                      ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                                      (leaf ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen : 0),
                                            "%s", label);
        // a click on the name picks it too; the arrow only folds
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            const float arrowEnd = ImGui::GetItemRectMin().x + ImGui::GetTreeNodeToLabelSpacing();
            if (leaf || ImGui::GetIO().MousePos.x > arrowEnd)
            {
                if (on) g_RunPick.erase(key); else g_RunPick.insert(key);
            }
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) RunPickNested(path);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Click: %s this folder\nRight-click: %s",
                              on ? "unpick" : "pick", leaf ? "(nothing inside it)" : "pick every folder inside it instead");
        if (open && !leaf)
        {
            RunPickTree(path, c, filtering);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

static void DrawRunFoldersWindow()
{
    RunFolderPicker& P = g_RunPicker;
    if (!P.open) return;
    if (!HasWorkspace() || g_App.folders.empty()) { P.open = false; return; }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (P.appearing)
    {
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowFocus();
        P.appearing = false;
    }
    ImGui::SetNextWindowSize(ImVec2(440 * g_Scale, 520 * g_Scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320 * g_Scale, 280 * g_Scale),
                                        ImVec2(ImMax(340 * g_Scale, vp->Size.x * 0.6f), vp->Size.y * 0.95f));
    if (!ImGui::Begin("Run against folders###runfolders", &P.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    {
        ImGui::End();
        return;
    }

    // who plays, in one line
    {
        std::string who;
        const int shown = ImMin(3, (int)P.groups.size());
        for (int i = 0; i < shown; i++) { if (i) who += ", "; who += P.groups[(size_t)i].name; }
        if ((int)P.groups.size() > shown) who += " and " + std::to_string((int)P.groups.size() - shown) + " more";
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(Hex(Col::TextDim), "%s play%s each picked folder as a tournament of its own.",
                           who.c_str(), P.groups.size() == 1 ? "s" : "");
        ImGui::PopTextWrapPos();
    }

    // the filter, and a few ways to pick many at once
    ImGui::SetNextItemWidth(ImMax(120 * g_Scale, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##filter", "Filter folders...", P.filter, sizeof(P.filter));
    if (ImGui::SmallButton("Top folders"))
        for (int fi : FolderChildren(std::string())) g_RunPick.insert(ToLower(g_App.folders[fi].path));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pick every folder at the top of the workspace");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) g_RunPick.clear();
    ImGui::SameLine();
    if (ImGui::SmallButton("Expand all")) P.openAll = 1;
    ImGui::SameLine();
    if (ImGui::SmallButton("Collapse all")) P.openAll = -1;

    // counts for the labels, worked out once a frame rather than once a row
    RunPickCounts c;
    const std::string filter = ToLower(P.filter);
    {
        std::set<std::string> known;
        for (const GroupFolder& f : g_App.folders)
        {
            const std::string k = ToLower(f.path);
            known.insert(k);
            if (!filter.empty() && k.find(filter) != std::string::npos)
                for (std::string up = k; !up.empty(); up = FolderParent(up)) c.shown.insert(up);
        }
        for (const Player& pl : g_App.players)
        {
            const std::string f = ToLower(pl.folder);
            if (f.empty() || !known.count(f)) continue;
            for (std::string up = f; !up.empty(); up = FolderParent(up)) c.groups[up]++;
        }
        for (const std::string& k : g_RunPick)
            for (std::string up = FolderParent(k); !up.empty(); up = FolderParent(up)) c.picked[up]++;
    }

    const float footH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y + 4 * g_Scale;
    ImGui::BeginChild("##runtree", ImVec2(0, ImMax(80.0f * g_Scale, ImGui::GetContentRegionAvail().y - footH)),
                      ImGuiChildFlags_Borders);
    if (!filter.empty() && c.shown.empty()) ImGui::TextDisabled("No folder matches \"%s\"", P.filter);
    RunPickTree(std::string(), c, !filter.empty());
    ImGui::EndChild();
    P.openAll = 0;

    // picks of folders that have gone since are left out
    std::vector<std::string> picked;
    for (const GroupFolder& f : g_App.folders) if (g_RunPick.count(ToLower(f.path))) picked.push_back(f.path);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Hex(Col::TextDim), "%d folder%s picked", (int)picked.size(), picked.size() == 1 ? "" : "s");
    if (!picked.empty() && ImGui::IsItemHovered())
    {
        std::string all;
        for (size_t i = 0; i < picked.size() && i < 40; i++) all += picked[i] + "\n";
        if (picked.size() > 40) all += "...";
        ImGui::SetTooltip("%s", all.c_str());
    }
    const float runW = ImGui::CalcTextSize("Run").x + 34 * g_Scale;
    const float cancelW = ImGui::CalcTextSize("Cancel").x + 24 * g_Scale;
    ImGui::SameLine(ImMax(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - runW - cancelW - 6 * g_Scale));
    if (ImGui::Button("Cancel", ImVec2(cancelW, 0))) P.open = false;
    ImGui::SameLine(0, 6 * g_Scale);
    ImGui::BeginDisabled(picked.empty());
    ImGui::PushStyleColor(ImGuiCol_Button, Hex(Col::Accent, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(Col::Accent, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(Col::Accent, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::Bg0));
    if (ImGui::Button("Run", ImVec2(runW, 0)))
    {
        WsOp op;
        op.kind = WsOp::RunFolders;
        op.items = P.groups;
        op.paths = picked;
        QueueWsOp(op);
        P.open = false;
    }
    ImGui::PopStyleColor(4);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("A folder's groups include the folders inside it.\n"
                          "Plays on the Competition panel's settings, with the C++ engine.");
    ImGui::End();
}

// The menu for a few rows at once: only what makes sense for all of them.
static void SharedContextMenu(const Ref& row)
{
    const std::vector<RefKey> items = ItemsForAction(row);
    ImGui::TextDisabled("%d selected", (int)g_App.selection.size());
    ImGui::Separator();
    bool allActive = true;
    for (const Ref& r : g_App.selection) if (!RefEnabled(r)) allActive = false;
    if (ImGui::MenuItem("Active", "Ctrl+Q", allActive)) SetSelectionActive(!allActive);
    if (ImGui::MenuItem("Only these active")) ActivateOnlySelection();
    ImGui::Separator();
    bool movable = false;
    for (const RefKey& k : items) if (k.kind == 0 || k.kind == 2) movable = true;
    if (movable)
    {
        std::vector<RefKey> mv;
        for (const RefKey& k : items) if (k.kind == 0 || k.kind == 2) mv.push_back(k);
        MoveToFolderMenu(mv, std::string("\x01"));
        RunAgainstFoldersMenu(items);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Export...")) ExportOpen(ExportKeysFor(row));
    ImGui::Separator();
    if (ImGui::MenuItem("Delete", "Del")) QueueDelete(items);
}

// The name of a row being renamed, as a text field over where the name sits.
// Enter or clicking away keeps it, Esc throws it away.
static void InlineRenameField(const ImVec2& mn, const ImVec2& mx)
{
    const float h = ImGui::GetFrameHeight();
    ImGui::SetCursorScreenPos(ImVec2(mn.x, (mn.y + mx.y - h) * 0.5f));
    ImGui::SetNextItemWidth(ImMax(40.0f * g_Scale, mx.x - mn.x));
    if (g_Rename.focus) { ImGui::SetKeyboardFocusHere(); g_Rename.focus = false; }
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Hex(Col::Bg0));
    ImGui::PushStyleColor(ImGuiCol_Border, Hex(Col::Accent, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool enter = ImGui::InputText("##inlinerename", g_Rename.buf, sizeof(g_Rename.buf),
                                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    const bool escaped = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (enter || (ImGui::IsItemDeactivated() && !escaped))
    {
        WsOp op;
        op.kind = WsOp::Rename;
        op.items.push_back(g_Rename.key);
        op.name = g_Rename.buf;
        QueueWsOp(op);
        g_Rename = InlineRename();
    }
    else if (escaped) g_Rename = InlineRename();
}

// A row that was just deleted, struck through and fading, with a way back.
static void DrawGhostRow(const GhostRow& g, float alpha, bool newest, float rowH)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 mn = o, mx(o.x + w, o.y + rowH);
    dl->AddRectFilled(mn, mx, HexU(Col::Danger, 0.07f * alpha), 7 * g_Scale);
    const float av = 20.0f * g_Scale;
    const ImVec2 amn(mn.x + 6 * g_Scale, mn.y + (rowH - av) * 0.5f), amx(amn.x + av, amn.y + av);
    if (g.kind == 1) DrawGlyphFolder(dl, ImVec2(amn.x + av * 0.5f, amn.y + av * 0.5f), 5.0f * g_Scale, HexU(Col::TextDim, alpha * 0.7f));
    else             dl->AddRect(amn, amx, HexU(g.tint, 0.5f * alpha), 5 * g_Scale, 0, 1.2f);
    const ImVec2 ts = ImGui::CalcTextSize(g.label.c_str());
    const float tx = amx.x + 9 * g_Scale, ty = mn.y + (rowH - ts.y) * 0.5f;
    const float clipX = newest ? mx.x - ImGui::CalcTextSize("Undo").x - 24 * g_Scale : mx.x - 6 * g_Scale;
    dl->PushClipRect(mn, ImVec2(clipX, mx.y), true);
    dl->AddText(ImVec2(tx, ty), HexU(Col::TextDim, 0.8f * alpha), g.label.c_str());
    dl->AddLine(ImVec2(tx, ty + ts.y * 0.55f), ImVec2(tx + ts.x, ty + ts.y * 0.55f), HexU(Col::Danger, 0.8f * alpha), 1.2f);
    dl->PopClipRect();
    if (newest)
    {
        const char* u = "Undo";
        const ImVec2 us = ImGui::CalcTextSize(u);
        ImGui::SetCursorScreenPos(ImVec2(mx.x - us.x - 16 * g_Scale, mn.y + (rowH - us.y - 4 * g_Scale) * 0.5f));
        ImGui::PushID(g.label.c_str());
        BtnState b = BareButton("##undo", ImVec2(us.x + 10 * g_Scale, us.y + 4 * g_Scale));
        dl->AddRectFilled(b.mn, b.mx, HexU(b.hovered ? Col::Bg3 : Col::Bg2, alpha), 5 * g_Scale);
        dl->AddText(ImVec2(b.mn.x + 5 * g_Scale, b.mn.y + 2 * g_Scale), HexU(Col::Accent, alpha), u);
        if (b.hovered) ImGui::SetTooltip("Bring it back (Ctrl+Z)");
        if (b.clicked) QueueWsOp(WsOp::Undo);
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(o.x, mx.y + 2 * g_Scale));
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::SetCursorScreenPos(ImVec2(o.x, mx.y + 2 * g_Scale));
}

// The ghosts of one list (kind 0 groups / 1 folders of `folder`), by the
// position they had.
struct GhostRef { const GhostRow* row; float alpha; bool newest; };
static std::vector<GhostRef> GhostsOf(int kind, const std::string& folder)
{
    std::vector<GhostRef> out;
    for (size_t b = 0; b < g_Trash.size(); b++)
    {
        const float a = GhostAlpha(g_Trash[b]);
        if (a <= 0.0f) continue;
        for (const GhostRow& g : g_Trash[b].ghosts)
            if (g.kind == kind && ToLower(g.folder) == ToLower(folder))
                out.push_back(GhostRef{ &g, a, b + 1 == g_Trash.size() });
    }
    return out;
}

static void SurvivorSlotButton(int pi, int si, float size, ImDrawFlags corners = ImDrawFlags_RoundCornersAll)
{
    AppState& S = g_App;
    Player& pl = S.players[pi];
    Survivor& sv = pl.slots[si];
    Ref ref; ref.player = pi; ref.slot = si;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(si);
    BtnState b = BareButton("##slot", ImVec2(size, size));
    const unsigned slotTint = pl.twoTone ? (si == 1 ? pl.tint2 : pl.tint)
                                         : SlotTint(pl.tint, si);

    const bool selected = IsSelected(ref);
    const bool isOpen   = (S.open == ref);
    const bool active   = sv.present && sv.enabled && pl.enabled;

    bool clicked = b.clicked;
    if (sv.present && CheckLongPress(clicked))
        ToggleRefEnabled(ref);

    // Active: the solid swatch. Not active: the same colour as an outline only,
    // so what fights reads at a glance and nothing looks broken.
    ImU32 bg, fg, border = 0;
    float borderW = 1.0f;
    if (!sv.present)
    {
        bg = HexU(Col::Bg2, 0.35f);
        fg = HexU(Col::TextDim, 0.55f);
        border = HexU(Col::Line);
    }
    else if (!active)
    {
        bg = HexU(b.hovered ? Col::Bg3 : Col::Bg2, selected ? 1.0f : 0.7f);
        fg = HexU(Col::TextDim);
        border = HexU(slotTint, selected ? 0.95f : 0.55f);
        borderW = selected ? 1.5f : 1.0f;
    }
    else if (selected)
    {
        bg = HexU(b.held ? Col::Bg4 : (b.hovered ? Col::Bg3 : Col::Bg2));
        fg = HexU(slotTint);
        border = HexU(slotTint, 0.95f);
    }
    else
    {
        bg = HexU(slotTint, b.hovered ? 1.0f : 0.85f);
        fg = HexU(Col::Bg0);
    }

    const float r = 6.0f * g_Scale;
    dl->AddRectFilled(b.mn, b.mx, bg, r, corners);
    if (border) dl->AddRect(b.mn, b.mx, border, r, corners, borderW);
    if (isOpen && active && !selected)
        dl->AddRect(b.mn, b.mx, HexU(slotTint, 0.85f), r, corners, 1.5f);

    const char label[2] = { (char)('1' + si), 0 };
    const ImVec2 tsz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2((b.mn.x + b.mx.x - tsz.x) * 0.5f, (b.mn.y + b.mx.y - tsz.y) * 0.5f), fg, label);

    // unsaved changes: a dot in the corner, ringed so it reads on any tint
    if (sv.present && sv.dirty)
    {
        const ImVec2 dc(b.mx.x - 4.0f * g_Scale, b.mn.y + 4.0f * g_Scale);
        dl->AddCircleFilled(dc, 3.2f * g_Scale, HexU(Col::Bg0));
        dl->AddCircleFilled(dc, 2.2f * g_Scale, HexU(Col::Warn));
    }

    DrawLongPressHint(b.mn, b.mx);

    if (b.hovered)
    {
        if (!sv.present)
            ImGui::SetTooltip("%s %d - empty\nThis group has %d survivor%s",
                              pl.name.c_str(), si + 1, pl.LoadedCount(), pl.LoadedCount() == 1 ? "" : "s");
        else
            ImGui::SetTooltip("%s  -  %s\n%s%s\n\nClick: open + select\nCtrl / Shift + click: multi-select\nHold: %s",
                              sv.name.c_str(), active ? "active" : "not active",
                              sv.asmPath.c_str(),
                              sv.dirty ? (g_App.autoSave
                                              ? "\nYellow dot: changes not on disk yet (autosave is on)"
                                              : "\nYellow dot: unsaved changes - Ctrl+S saves")
                                       : "",
                              active ? "take it out" : "make it active");
    }

    if (clicked && sv.present)
    {
        HandleRowClick(ref, true);
        if (S.game.ready())
        {
            int w = WarriorOfPlayer(pi, si);
            if (w < 0)
            {
                // fall back to the warrior file name, in case the group was
                // renamed after the war was recorded
                for (int k = 0; k < S.game.numWarriors(); k++)
                    if (S.game.replay().warriorName[(size_t)k] == sv.name) { w = k; break; }
            }
            if (w >= 0) S.inspectWarrior = w;
            S.status = (w >= 0) ? ("Inspecting " + S.game.replay().warriorName[(size_t)w])
                                : ("No warrior in this war for " + pl.name + " slot " + std::to_string(si + 1));
        }
    }

    if (sv.present && ImGui::BeginPopupContextItem("##slotctx"))
    {
        if (MultiSelected(ref)) SharedContextMenu(ref);
        else
        {
            ImGui::TextDisabled("%s", sv.name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Active", "Ctrl+Q", active)) ToggleRefEnabled(ref);
            if (ImGui::MenuItem("Save", "Ctrl+S")) SaveSurvivor(ref);
            if (ImGui::MenuItem("Export...")) ExportOpen(ExportKeysFor(ref));
            if (ImGui::MenuItem("Reveal in Explorer")) RevealInExplorer(PathExists(sv.asmPath) ? sv.asmPath : sv.binPath);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete this survivor", "Del"))
            {
                RefKey k = KeyOfRef(ref);
                QueueDelete(std::vector<RefKey>(1, k));
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

// The drag payload of a row: which row was picked up.
static void RowDragSource(const Ref& row, const char* label)
{
    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) return;
    ImGui::SetDragDropPayload("CW_ROW", &row, sizeof(row));
    const std::vector<RefKey> items = ItemsForAction(row);
    if (items.size() == 1) ImGui::TextUnformatted(label);
    else                   ImGui::Text("%d items", (int)items.size());
    ImGui::EndDragDropSource();
}

// A row things can be dropped on: they go into `folder`.
static bool RowDropTarget(const std::string& folder)
{
    bool hot = false;
    if (ImGui::BeginDragDropTarget())
    {
        hot = true;
        if (const ImGuiPayload* pay = ImGui::AcceptDragDropPayload("CW_ROW"))
            QueueMove(ItemsForAction(*(const Ref*)pay->Data), folder);
        ImGui::EndDragDropTarget();
    }
    return hot;
}

static void DrawPlayerRow(int pi, float rowH)
{
    AppState& S = g_App;
    Player& pl = S.players[pi];
    Ref ref; ref.player = pi; ref.slot = -1;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float  avail    = ImGui::GetContentRegionAvail().x;
    const float  slotSize = 21.0f * g_Scale;
    const float  slotGap  = 2.0f * g_Scale;     // the two survivors are one train
    const float  gap      = 5.0f * g_Scale;

    ImGui::PushID(pi);

    // The row draws its own states, so ImGui's selection fill is turned off.
    const bool selected = IsSelected(ref);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
    bool clicked = ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowOverlap, ImVec2(avail, rowH));
    ImGui::PopStyleColor(3);
    const ImVec2 rowMin = ImGui::GetItemRectMin();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    const bool rowHovered = ImGui::IsItemHovered();
    const bool rowHeld = ImGui::IsItemActive();
    AddDropZone(rowMin, rowMax, pl.folder, false);

    // a group row can be dragged onto a folder, or onto another group to join
    // whatever folder that one is in
    RowDragSource(ref, pl.name.c_str());
    const bool dropHot = RowDropTarget(pl.folder);

    if (CheckLongPress(clicked))
        ToggleRefEnabled(ref);

    // row surface: nothing at rest, a soft fill on hover, the accent when picked
    const float rr = 7.0f * g_Scale;
    if (selected)
    {
        dl->AddRectFilled(rowMin, rowMax, HexU(Col::Accent, rowHovered ? 0.20f : 0.15f), rr);
        dl->AddRect(rowMin, rowMax, HexU(Col::Accent, 0.35f), rr, 0, 1.0f);
    }
    else if (rowHovered || rowHeld)
        dl->AddRectFilled(rowMin, rowMax, HexU(rowHeld ? Col::Bg3 : Col::Bg2), rr);
    if (dropHot) dl->AddRect(rowMin, rowMax, HexU(Col::Accent, 0.6f), rr, 0, 1.2f);
    DrawLongPressHint(rowMin, rowMax);

    const bool renaming = RenamingRef(ref);
    const float  textX    = rowMin.x + 6 * g_Scale + 20.0f * g_Scale + 9 * g_Scale;
    const float  nameClip = rowMax.x - slotSize * 2 - slotGap - gap * 2;
    if (!renaming)
    {
        const ImVec2 nsz   = ImGui::CalcTextSize(pl.name.c_str());
        const float  textY = rowMin.y + (rowH - nsz.y) * 0.5f;
        dl->PushClipRect(ImVec2(rowMin.x, rowMin.y), ImVec2(nameClip, rowMax.y), true);
        dl->AddText(ImVec2(textX, textY), HexU(pl.enabled ? Col::Text : Col::TextDim), pl.name.c_str());
        dl->PopClipRect();
    }

    if (rowHovered && !ImGui::IsAnyItemActive())
        ImGui::SetTooltip("%s - %s, %d/%d survivors%s\n\nClick: select   Ctrl / Shift + click: multi-select\n"
                          "Click the square or hold the row: %s\nF2: rename   Del: delete   drag: move to a folder",
                          pl.name.c_str(), pl.enabled ? "active" : "not active", pl.LoadedCount(), kSlotCount,
                          pl.folder.empty() ? "" : ("\nin " + pl.folder).c_str(),
                          pl.enabled ? "take it out" : "make it active");

    if (ImGui::BeginPopupContextItem("##rowctx"))
    {
        if (MultiSelected(ref)) SharedContextMenu(ref);
        else
        {
            ImGui::TextDisabled("%s", pl.name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2")) BeginInlineRename(ref);
            auto colourPick = [](const char* label, unsigned& v) {
                float c[3] = { ((v >> 16) & 0xFF) / 255.0f,
                               ((v >>  8) & 0xFF) / 255.0f,
                               ( v        & 0xFF) / 255.0f };
                if (ImGui::ColorEdit3(label, c,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel))
                    v = ((unsigned)(c[0] * 255.0f + 0.5f) << 16) |
                        ((unsigned)(c[1] * 255.0f + 0.5f) <<  8) |
                         (unsigned)(c[2] * 255.0f + 0.5f);
            };
            // One colour lightened for survivor 2, or two colours of your own.
            if (ImGui::Checkbox("Two colours", &pl.twoTone) && pl.twoTone)
                pl.tint2 = SlotTint(pl.tint, 1);
            if (pl.twoTone)
            {
                colourPick("Survivor 1", pl.tint);
                colourPick("Survivor 2", pl.tint2);
            }
            else colourPick("Colour", pl.tint);
            ImGui::Separator();
            if (ImGui::MenuItem("Active", "Ctrl+Q", pl.enabled)) ToggleRefEnabled(ref);
            if (ImGui::MenuItem("Only this one active"))
            {
                S.selection.assign(1, ref);
                ActivateOnlySelection();
            }
            const std::vector<RefKey> items = ItemsForAction(ref);
            MoveToFolderMenu(items, pl.folder);
            RunAgainstFoldersMenu(items);
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) { S.selection.assign(1, ref); CopySelectionToClipboard(); }
            if (ImGui::MenuItem("Export...")) ExportOpen(ExportKeysFor(ref));
            if (ImGui::MenuItem("Reveal in Explorer"))
            {
                const Survivor* first = pl.FirstLoaded() >= 0 ? &pl.slots[pl.FirstLoaded()] : NULL;
                if (first) RevealInExplorer(PathExists(first->asmPath) ? first->asmPath : first->binPath);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del")) QueueDelete(items);
        }
        ImGui::EndPopup();
    }

    // the avatar: the group's colour (split when it has two) with its initial,
    // solid when the group is active and an outline when it is not -- and a
    // click on it is the switch
    {
        const float av = 20.0f * g_Scale;
        const ImVec2 amn(rowMin.x + 6 * g_Scale, rowMin.y + (rowH - av) * 0.5f);
        ImGui::SetCursorScreenPos(amn);
        BtnState ab = BareButton("##avatar", ImVec2(av, av));
        const ImVec2 amx = ab.mx;
        const float ar = 5.0f * g_Scale;
        if (pl.enabled)
        {
            const float lift = ab.hovered ? 1.0f : 0.92f;
            if (pl.twoTone)
            {
                const float mid = ImFloor((amn.x + amx.x) * 0.5f);
                dl->AddRectFilled(amn, ImVec2(mid, amx.y), HexU(pl.tint, lift), ar, ImDrawFlags_RoundCornersLeft);
                dl->AddRectFilled(ImVec2(mid, amn.y), amx, HexU(pl.tint2, lift), ar, ImDrawFlags_RoundCornersRight);
            }
            else dl->AddRectFilled(amn, amx, HexU(pl.tint, lift), ar);
        }
        else
        {
            dl->AddRectFilled(amn, amx, HexU(ab.hovered ? Col::Bg3 : Col::Bg2), ar);
            dl->AddRect(amn, amx, HexU(pl.tint, ab.hovered ? 0.9f : 0.6f), ar, 0, 1.5f);
        }
        char ini[2] = { pl.name.empty() ? '?' : (char)toupper((unsigned char)pl.name[0]), 0 };
        const ImVec2 is = ImGui::CalcTextSize(ini);
        dl->AddText(ImVec2((amn.x + amx.x - is.x) * 0.5f, (amn.y + amx.y - is.y) * 0.5f),
                    pl.enabled ? HexU(Col::Bg0, 0.9f) : HexU(pl.tint, 0.85f), ini);
        if (ab.hovered)
            ImGui::SetTooltip(pl.enabled ? "Active - fights in Debug and Competition\nClick: take it out"
                                         : "Not active - sits out\nClick: make it active");
        if (ab.clicked) ToggleRefEnabled(ref);
    }

    if (renaming) InlineRenameField(ImVec2(textX - 4 * g_Scale, rowMin.y), ImVec2(nameClip, rowMax.y));

    // the two survivor buttons, one train, submitted after the row so they win
    // hover and click
    float bx = rowMax.x - (rowH - slotSize) * 0.5f - slotSize * 2 - slotGap;
    const float by = rowMin.y + (rowH - slotSize) * 0.5f;
    for (int sl = 0; sl < kSlotCount; sl++)
    {
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        SurvivorSlotButton(pi, sl, slotSize, TrainCorners(sl, kSlotCount));
        bx += slotSize + slotGap;
    }

    ImGui::PopID();

    if (clicked && !renaming)
    {
        HandleRowClick(ref, true);
        g_GroupsFocus = true;
    }

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowMax.y + 2 * g_Scale));
}

// A folder with a small plus in its corner: "new folder".
static void DrawGlyphFolderPlus(ImDrawList* dl, const ImVec2& c, float s, ImU32 col)
{
    DrawGlyphFolder(dl, ImVec2(c.x - s * 0.15f, c.y + s * 0.1f), s * 0.85f, col);
    const ImVec2 k(c.x + s * 0.85f, c.y - s * 0.75f);
    dl->AddLine(ImVec2(k.x - s * 0.45f, k.y), ImVec2(k.x + s * 0.45f, k.y), col, 1.4f);
    dl->AddLine(ImVec2(k.x, k.y - s * 0.45f), ImVec2(k.x, k.y + s * 0.45f), col, 1.4f);
}

// The heading of a folder of groups: a caret, the name and how many of the
// groups in it (folders inside included) are active. Clicking selects it and
// folds it; holding it switches every group in it.
static void DrawFolderRow(int fi, float rowH)
{
    AppState& S = g_App;
    GroupFolder& f = S.folders[fi];
    const std::string path = f.path;
    const std::vector<int> members = GroupsUnder(path);
    int on = 0;
    for (int p : members) if (S.players[p].enabled) on++;
    Ref ref; ref.player = kFolderPlayer; ref.slot = fi;
    const bool selected = IsSelected(ref);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float  avail    = ImGui::GetContentRegionAvail().x;

    ImGui::PushID("folder");
    ImGui::PushID(path.c_str());
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
    bool clicked = ImGui::Selectable("##frow", selected, ImGuiSelectableFlags_AllowOverlap, ImVec2(avail, rowH));
    ImGui::PopStyleColor(3);
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    AddDropZone(mn, mx, path, false);
    if (selected)
    {
        dl->AddRectFilled(mn, mx, HexU(Col::Accent, hovered ? 0.20f : 0.15f), 7.0f * g_Scale);
        dl->AddRect(mn, mx, HexU(Col::Accent, 0.35f), 7.0f * g_Scale, 0, 1.0f);
    }
    else if (hovered || ImGui::IsItemActive())
        dl->AddRectFilled(mn, mx, HexU(ImGui::IsItemActive() ? Col::Bg3 : Col::Bg2), 7.0f * g_Scale);

    RowDragSource(ref, FolderLeaf(path).c_str());
    const bool dropHot = RowDropTarget(path);

    if (CheckLongPress(clicked)) ToggleRefEnabled(ref);
    DrawLongPressHint(mn, mx);

    const bool dim = !members.empty() && on == 0;
    const ImU32 ink = HexU(dim ? Col::TextDim : Col::Text, dim ? 0.8f : 1.0f);
    if (dropHot) dl->AddRect(mn, mx, HexU(Col::Accent, 0.9f), 5 * g_Scale, 0, 1.5f);

    // caret
    {
        const ImVec2 c(mn.x + 8 * g_Scale, (mn.y + mx.y) * 0.5f);
        const float a = 3.5f * g_Scale;
        if (f.collapsed)
            dl->AddTriangleFilled(ImVec2(c.x - a * 0.7f, c.y - a), ImVec2(c.x + a, c.y),
                                  ImVec2(c.x - a * 0.7f, c.y + a), HexU(Col::TextDim));
        else
            dl->AddTriangleFilled(ImVec2(c.x - a, c.y - a * 0.7f), ImVec2(c.x + a, c.y - a * 0.7f),
                                  ImVec2(c.x, c.y + a), HexU(Col::TextDim));
    }
    DrawGlyphFolder(dl, ImVec2(mn.x + 24 * g_Scale, (mn.y + mx.y) * 0.5f), 5.5f * g_Scale,
                    HexU(dim ? Col::TextDim : Col::Accent, dim ? 0.7f : 0.95f));

    // how many are active, out of how many
    char cnt[24];
    if (members.empty())                   snprintf(cnt, sizeof(cnt), "0");
    else if (on == (int)members.size())    snprintf(cnt, sizeof(cnt), "%d", (int)members.size());
    else                                   snprintf(cnt, sizeof(cnt), "%d/%d", on, (int)members.size());
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.86f);
    const ImVec2 cs = ImGui::CalcTextSize(cnt);
    const float  pillW = cs.x + 12 * g_Scale, pillH = cs.y + 3 * g_Scale;
    const ImVec2 pmn(mx.x - pillW - 6 * g_Scale, (mn.y + mx.y - pillH) * 0.5f);
    dl->AddRectFilled(pmn, ImVec2(pmn.x + pillW, pmn.y + pillH), HexU(Col::Bg3, 0.8f), pillH * 0.5f);
    dl->AddText(ImVec2(pmn.x + 6 * g_Scale, pmn.y + 1.5f * g_Scale), HexU(Col::TextDim), cnt);
    ImGui::PopFont();

    const bool renaming = RenamingRef(ref);
    const std::string leaf = FolderLeaf(path);
    if (!renaming)
    {
        const ImVec2 ns = ImGui::CalcTextSize(leaf.c_str());
        dl->PushClipRect(mn, ImVec2(pmn.x - 6 * g_Scale, mx.y), true);
        dl->AddText(ImVec2(mn.x + 36 * g_Scale, mn.y + (rowH - ns.y) * 0.5f), ink, leaf.c_str());
        dl->PopClipRect();
    }

    if (hovered && !ImGui::IsAnyItemActive() && !renaming)
        ImGui::SetTooltip("%s - %d group(s), %d active\n\nClick: select and %s\nHold: %s every group in it\n"
                          "Drop groups or folders here to move them in\nF2: rename   Del: delete",
                          path.c_str(), (int)members.size(), on,
                          f.collapsed ? "unfold" : "fold", on ? "take out" : "make active");

    if (ImGui::BeginPopupContextItem("##fctx"))
    {
        if (MultiSelected(ref)) SharedContextMenu(ref);
        else
        {
            ImGui::TextDisabled("%s", path.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2")) BeginInlineRename(ref);
            if (ImGui::MenuItem("New folder inside")) QueueWsOp(WsOp::NewFolder, path);
            if (ImGui::MenuItem("New group inside"))  QueueWsOp(WsOp::NewGroup, path);
            ImGui::Separator();
            if (ImGui::MenuItem("Every group active", NULL, !members.empty() && on == (int)members.size(),
                                !members.empty()))
                ToggleRefEnabled(ref);
            if (ImGui::MenuItem("Only this folder active", NULL, false, !members.empty()))
            {
                S.selection.assign(1, ref);
                ActivateOnlySelection();
            }
            if (ImGui::MenuItem("Select every group", NULL, false, !members.empty()))
            {
                S.selection.clear();
                for (int p : members) { Ref r; r.player = p; r.slot = -1; S.selection.push_back(r); }
                S.anchor = S.selection.back();
            }
            const std::vector<RefKey> items = ItemsForAction(ref);
            MoveToFolderMenu(items, FolderParent(path));
            ImGui::Separator();
            if (ImGui::MenuItem("Paste here", "Ctrl+V")) QueueWsOp(WsOp::Paste, path);
            if (ImGui::MenuItem("Export...")) ExportOpen(ExportKeysFor(ref));
            if (ImGui::MenuItem("Reveal in Explorer")) RevealInExplorer(TreeDir(kSurvDir, path));
            if (ImGui::MenuItem("Ungroup  (keep what is inside)")) QueueWsOp(WsOp::Ungroup, path);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete folder", "Del")) QueueDelete(items);
        }
        ImGui::EndPopup();
    }

    if (renaming) InlineRenameField(ImVec2(mn.x + 32 * g_Scale, mn.y), ImVec2(pmn.x - 6 * g_Scale, mx.y));

    ImGui::PopID();
    ImGui::PopID();

    if (clicked && !renaming)
    {
        const ImGuiIO& io = ImGui::GetIO();
        HandleRowClick(ref, false);
        if (!io.KeyCtrl && !io.KeyShift && fi < (int)S.folders.size()) S.folders[fi].collapsed = !S.folders[fi].collapsed;
        g_GroupsFocus = true;
    }
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, mx.y + 2 * g_Scale));
}

// A row scrolled out of sight only takes its room. Drawing every one of a
// thousand groups -- buttons, menus, tooltips -- was most of a frame. Rows still
// draw in full while they are being renamed or while anything is dragged.
static bool RowOutOfSight(const Ref& ref, float rowH)
{
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (ImGui::IsRectVisible(at, ImVec2(at.x + 4.0f, at.y + rowH))) return false;
    if (RenamingRef(ref) || ImGui::GetDragDropPayload() != NULL) return false;
    ImGui::Dummy(ImVec2(0.0f, rowH));
    ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + rowH + 2 * g_Scale));   // a drawn row's pitch
    return true;
}

// One level of the tree: its folders (each followed by what is inside it,
// hanging off a guide line), then its groups -- with the rows deleted a moment
// ago still showing where they were.
static void DrawFolderTree(const std::string& parent, float rowH)
{
    AppState& S = g_App;
    const std::vector<GhostRef> folderGhosts = GhostsOf(1, parent);
    const std::vector<int> kids = FolderChildren(parent);
    auto ghostsAt = [&](const std::vector<GhostRef>& gs, int pos) {
        for (const GhostRef& g : gs) if (g.row->pos == pos) DrawGhostRow(*g.row, g.alpha, g.newest, rowH);
    };
    for (int i = 0; i < (int)kids.size(); i++)
    {
        ghostsAt(folderGhosts, i);
        const int fi = kids[i];
        if (fi >= (int)S.folders.size()) break;
        const std::string path = S.folders[fi].path;
        {
            Ref fref; fref.player = kFolderPlayer; fref.slot = fi;
            if (!RowOutOfSight(fref, rowH)) DrawFolderRow(fi, rowH);
        }
        const int now = FolderIndex(path);          // a click may have folded it
        if (now < 0 || S.folders[now].collapsed) continue;
        const float guideX = ImGui::GetCursorScreenPos().x + 8 * g_Scale;
        const float top = ImGui::GetCursorScreenPos().y;
        ImGui::Indent(14 * g_Scale);
        DrawFolderTree(path, rowH);
        ImGui::Unindent(14 * g_Scale);
        const float bottom = ImGui::GetCursorScreenPos().y - 4 * g_Scale;
        if (bottom > top)
            ImGui::GetWindowDrawList()->AddLine(ImVec2(guideX, top), ImVec2(guideX, bottom), HexU(Col::Line), 1.0f);
    }
    for (const GhostRef& g : folderGhosts) if (g.row->pos >= (int)kids.size()) DrawGhostRow(*g.row, g.alpha, g.newest, rowH);

    const std::vector<GhostRef> groupGhosts = GhostsOf(0, parent);
    const std::vector<int> groups = GroupsIn(parent);
    if (!kids.empty() && !groups.empty() && parent.empty())
        ImGui::Dummy(ImVec2(0, 3 * g_Scale));     // a breath between the folders and the loose groups
    for (int i = 0; i < (int)groups.size(); i++)
    {
        ghostsAt(groupGhosts, i);
        if (groups[i] >= (int)S.players.size()) continue;
        Ref gref; gref.player = groups[i]; gref.slot = -1;
        if (!RowOutOfSight(gref, rowH)) DrawPlayerRow(groups[i], rowH);
    }
    for (const GhostRef& g : groupGhosts) if (g.row->pos >= (int)groups.size()) DrawGhostRow(*g.row, g.alpha, g.newest, rowH);
}

// The workspace switcher under the header's folder button.
static void WorkspaceMenu()
{
    AppState& S = g_App;
    if (HasWorkspace())
    {
        ImGui::TextDisabled("%s", WsRoot().c_str());
        ImGui::Separator();
    }
    if (ImGui::MenuItem("Open workspace...", "Ctrl+O")) QueueWsOp(WsOp::PickWorkspace);
    std::vector<std::string> recent;
    for (const std::string& r : S.recentWorkspaces)
        if (ToLower(r) != ToLower(WsRoot()) && DirExists(r)) recent.push_back(r);
    if (ImGui::BeginMenu("Open recent", !recent.empty()))
    {
        for (const std::string& r : recent)
        {
            if (ImGui::MenuItem((FileNameOf(r) + "##" + r).c_str()))
                QueueWsOp(WsOp::OpenWorkspace, r);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.c_str());
        }
        ImGui::EndMenu();
    }
    if (!HasWorkspace()) return;
    ImGui::Separator();
    if (ImGui::MenuItem("Paste files", "Ctrl+V")) QueueWsOp(WsOp::Paste, TargetFolder());
    if (ImGui::MenuItem("Reveal in Explorer")) RevealInExplorer(WsRoot());
    if (ImGui::MenuItem("Read the folder again")) ReconcileWorkspace();
    ImGui::Separator();
    if (ImGui::MenuItem("Close workspace")) QueueWsOp(WsOp::CloseWorkspace);
}

// No workspace yet: the only thing to do is pick one.
static void DrawWelcome()
{
    AppState& S = g_App;
    ImGui::Dummy(ImVec2(0, 10 * g_Scale));
    ImGui::TextUnformatted("No workspace open");
    ImGui::Dummy(ImVec2(0, 4 * g_Scale));
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
    ImGui::TextWrapped("A workspace is a folder: every group, zombie and folder you make lives in it "
                       "and is saved there as you go. Pick an empty folder to start a new one, or a "
                       "folder that already has survivors (and zombies) in it.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 10 * g_Scale));
    ImGui::PushStyleColor(ImGuiCol_Button, Hex(Col::Accent, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(Col::Accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(Col::Accent, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(0xFFFFFF));
    if (ImGui::Button("Open workspace...", ImVec2(-FLT_MIN, 30 * g_Scale))) QueueWsOp(WsOp::PickWorkspace);
    ImGui::PopStyleColor(4);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+O. Dropping a folder on the window opens it too.");

    std::vector<std::string> recent;
    for (const std::string& r : S.recentWorkspaces) if (DirExists(r)) recent.push_back(r);
    if (recent.empty()) return;
    ImGui::Dummy(ImVec2(0, 14 * g_Scale));
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
    ImGui::TextColored(Hex(Col::TextDim), "RECENT");
    ImGui::PopFont();
    for (const std::string& r : recent)
    {
        ImGui::PushID(r.c_str());
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x, h = 40 * g_Scale;
        BtnState b = BareButton("##recent", ImVec2(w, h));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(b.held ? Col::Bg3 : Col::Bg2), 7 * g_Scale);
        DrawGlyphFolder(dl, ImVec2(o.x + 14 * g_Scale, o.y + h * 0.5f), 5.5f * g_Scale, HexU(Col::Accent));
        dl->PushClipRect(b.mn, b.mx, true);
        dl->AddText(ImVec2(o.x + 30 * g_Scale, o.y + 4 * g_Scale), HexU(Col::Text), FileNameOf(r).c_str());
        ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
        dl->AddText(ImVec2(o.x + 30 * g_Scale, o.y + h * 0.52f), HexU(Col::TextDim), ParentDir(r).c_str());
        ImGui::PopFont();
        dl->PopClipRect();
        if (b.hovered) ImGui::SetTooltip("%s", r.c_str());
        if (b.clicked) QueueWsOp(WsOp::OpenWorkspace, r);
        ImGui::PopID();
    }
}

static void DrawWorkspacePanel(float width, float height)
{
    AppState& S = g_App;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    g_DropZones.swap(g_DropZonesBuild);
    g_DropZonesBuild.clear();

    dl->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), HexU(Col::Bg1));

    // The list keys follow the last click: in here, or anywhere else (a click
    // in one of the panel's own menus counts as in here).
    // (a floating window lying over the panel is not the panel)
    if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) &&
        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        g_GroupsFocus = ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + width, origin.y + height), false) &&
                        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    // --- header ------------------------------------------------------------
    const float headerH = 44.0f * g_Scale;
    {
        // the caret takes the left edge, so the title starts after it
        const float caretW = 24.0f * g_Scale;
        const float bs = 24.0f * g_Scale;
        BtnState b;
        // Collapsed there is only room for the caret, so nothing else is drawn:
        // the actions would sit on top of it.
        if (!S.groupsCollapsed)
        {
            // title, and how many groups are active out of how many
            const char* title = "Groups";
            const ImVec2 tsz = ImGui::CalcTextSize(title);
            const float tx = origin.x + caretW + 10 * g_Scale;
            const float ty = origin.y + (headerH - tsz.y) * 0.5f;
            dl->AddText(ImVec2(tx, ty), HexU(Col::Text), title);
            if (HasWorkspace())
            {
                int active = 0;
                for (const Player& pl : S.players) if (pl.enabled) active++;
                char n[24];
                if (active == (int)S.players.size()) snprintf(n, sizeof(n), "%d", (int)S.players.size());
                else                                  snprintf(n, sizeof(n), "%d/%d", active, (int)S.players.size());
                ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
                const ImVec2 ns = ImGui::CalcTextSize(n);
                const float ph = ns.y + 4 * g_Scale, pw = ImMax(ph, ns.x + 10 * g_Scale);
                const ImVec2 pmn(tx + tsz.x + 7 * g_Scale, origin.y + (headerH - ph) * 0.5f);
                dl->AddRectFilled(pmn, ImVec2(pmn.x + pw, pmn.y + ph),
                                  HexU(active == (int)S.players.size() ? Col::Bg3 : Col::Accent,
                                       active == (int)S.players.size() ? 1.0f : 0.30f), ph * 0.5f);
                dl->AddText(ImVec2(pmn.x + (pw - ns.x) * 0.5f, pmn.y + 2 * g_Scale), HexU(Col::TextDim), n);
                ImGui::PopFont();
                if (ImGui::IsMouseHoveringRect(pmn, ImVec2(pmn.x + pw, pmn.y + ph)))
                    ImGui::SetTooltip("%d of %d groups active", active, (int)S.players.size());
            }

            // the actions, as one train on the right
            struct Act { const char* id; int glyph; const char* tip; };
            static const Act acts[3] = {
                { "##wsnewfolder", 0, "New folder\nInside the selected folder. Drag groups onto it to file them." },
                { "##wsnew",       1, "New group (Ctrl+N)\nTwo survivors from the template, in the selected folder" },
                { "##wsload",      2, "Workspace\nOpen another (Ctrl+O), recent ones, reveal, close" },
            };
            const int nActs = HasWorkspace() ? 3 : 1;
            float bx = origin.x + width - bs * nActs - 10 * g_Scale;
            const float by = origin.y + (headerH - bs) * 0.5f;
            for (int i = 3 - nActs; i < 3; i++)
            {
                ImGui::SetCursorScreenPos(ImVec2(bx, by));
                b = BareButton(acts[i].id, ImVec2(bs, bs));
                dl->AddRectFilled(b.mn, b.mx, HexU(b.held ? Col::Bg4 : b.hovered ? Col::Bg3 : Col::Bg2),
                                  6 * g_Scale, TrainCorners(i - (3 - nActs), nActs));
                TrainJoint(dl, b.mn, b.mx, i - (3 - nActs));
                const ImVec2 c((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f);
                const ImU32 ink = HexU(b.hovered ? Col::Text : Col::TextDim);
                switch (acts[i].glyph)
                {
                    case 0: DrawGlyphFolderPlus(dl, c, 5 * g_Scale, ink); break;
                    case 1: DrawGlyphPlus(dl, c, 5 * g_Scale, ink);       break;
                    default: DrawGlyphFolder(dl, c, 5 * g_Scale, ink);    break;
                }
                if (b.hovered) ImGui::SetTooltip("%s", acts[i].tip);
                if (b.clicked)
                {
                    switch (i)
                    {
                        case 0: QueueWsOp(WsOp::NewFolder, TargetFolder()); break;
                        case 1: QueueWsOp(WsOp::NewGroup, TargetFolder()); break;
                        default: ImGui::OpenPopup("##wsmenu"); break;
                    }
                }
                bx += bs;
            }
            if (ImGui::BeginPopup("##wsmenu"))
            {
                WorkspaceMenu();
                ImGui::EndPopup();
            }
        }

        // collapse: names go, leaving one line of survivor chips per group
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 8 * g_Scale, origin.y + (headerH - bs) * 0.5f));
        b = BareButton("##wscollapse", ImVec2(bs, bs));
        if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3), 6 * g_Scale);
        {
            const ImVec2 c((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f);
            const float a = 4 * g_Scale;
            if (S.groupsCollapsed)
                dl->AddTriangleFilled(ImVec2(c.x - a, c.y - a), ImVec2(c.x + a, c.y),
                                      ImVec2(c.x - a, c.y + a), HexU(Col::TextDim));
            else
                dl->AddTriangleFilled(ImVec2(c.x - a, c.y - a), ImVec2(c.x + a, c.y - a),
                                      ImVec2(c.x, c.y + a), HexU(Col::TextDim));
        }
        if (b.hovered) ImGui::SetTooltip(S.groupsCollapsed ? "Show group names" : "Collapse to survivors only");
        if (b.clicked) S.groupsCollapsed = !S.groupsCollapsed;

        dl->AddLine(ImVec2(origin.x + 10 * g_Scale, origin.y + headerH - 1),
                    ImVec2(origin.x + width - 10 * g_Scale, origin.y + headerH - 1), HexU(Col::Line));
    }

    // --- list --------------------------------------------------------------
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + headerH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * g_Scale, 8 * g_Scale));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    // A borderless child drops WindowPadding unless it is asked to keep it, so
    // without this the group and zombie rows sat flush against the edges.
    ImGui::BeginChild("##wslist", ImVec2(width, height - headerH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);

    if (!HasWorkspace())
    {
        if (!S.groupsCollapsed) DrawWelcome();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SetCursorScreenPos(ImVec2(origin.x + width, origin.y));
        return;
    }

    // warriors dropped at the top of the workspace, outside its layout
    if (g_WsLooseFiles > 0 && !S.groupsCollapsed)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::Warn));
        ImGui::TextWrapped("%d warrior file(s) lie loose at the top of the workspace.", g_WsLooseFiles);
        ImGui::PopStyleColor();
        if (ImGui::SmallButton("Sort them in")) QueueWsOp(WsOp::SortLoose);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(".asm into assembly, machine code into survivors (zom* into zombies)");
        ImGui::Dummy(ImVec2(0, 4 * g_Scale));
    }

    const float rowH = 30.0f * g_Scale;
    if (S.groupsCollapsed)
    {
        // one line per group, its two survivors side by side; the zombies
        // follow under a rule of their own
        const float sz = kCollapsedChip * g_Scale, gap = 4.0f * g_Scale;
        const ImVec2 base = ImGui::GetCursorScreenPos();
        float y = 0.0f;
        for (const Ref& r : FlatRows())
        {
            if (IsFolderRef(r))
            {
                // a folder is one chip two survivors wide; clicking it folds its rows
                GroupFolder& f = S.folders[r.slot];
                const float ind = (float)(FolderDepth(f.path) - 1) * 2.0f * g_Scale;
                ImGui::PushID("folderchip");
                ImGui::PushID(f.path.c_str());
                ImGui::SetCursorScreenPos(ImVec2(base.x + ind, base.y + y));
                BtnState fb = BareButton("##fchip", ImVec2(sz * 2 + gap - ind, sz * 0.8f));
                dl->AddRectFilled(fb.mn, fb.mx, HexU(fb.hovered ? Col::Bg3 : Col::Bg2), 5 * g_Scale);
                DrawGlyphFolder(ImGui::GetWindowDrawList(), ImVec2((fb.mn.x + fb.mx.x) * 0.5f, (fb.mn.y + fb.mx.y) * 0.5f),
                                4.5f * g_Scale, HexU(f.collapsed ? Col::TextDim : Col::Accent));
                if (fb.hovered) ImGui::SetTooltip("%s - %d group(s)\nClick: %s", f.path.c_str(),
                                                  (int)GroupsUnder(f.path).size(), f.collapsed ? "unfold" : "fold");
                if (fb.clicked) f.collapsed = !f.collapsed;
                ImGui::PopID();
                ImGui::PopID();
                y += sz * 0.8f + gap;
                continue;
            }
            if (r.slot != -1) continue;
            // chips out of sight are only counted, not drawn
            if (ImGui::IsRectVisible(ImVec2(base.x, base.y + y), ImVec2(base.x + sz * 2 + gap, base.y + y + sz)) ||
                ImGui::GetDragDropPayload() != NULL)
            {
                ImGui::PushID(r.player);
                for (int sl = 0; sl < kSlotCount; sl++)
                {
                    ImGui::SetCursorScreenPos(ImVec2(base.x + sl * (sz + gap), base.y + y));
                    SurvivorSlotButton(r.player, sl, sz, TrainCorners(sl, kSlotCount));
                }
                ImGui::PopID();
            }
            y += sz + gap;
        }
        if (!S.zombies.empty())
        {
            const float lineY = base.y + y + gap;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(base.x, lineY), ImVec2(base.x + sz * 2 + gap, lineY), HexU(Col::Line));
            y += gap * 3;
            for (int zi = 0; zi < (int)S.zombies.size(); zi++)
            {
                ImGui::SetCursorScreenPos(ImVec2(base.x, base.y + y));
                ZombieChip(zi, sz);
                y += sz + gap;
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(base.x, base.y + y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    else
    {
        if (S.players.empty() && S.folders.empty() && GhostsOf(0, std::string()).empty() &&
            GhostsOf(1, std::string()).empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
            ImGui::TextWrapped("No groups yet. Make one with the + above (Ctrl+N), paste warrior files "
                               "here (Ctrl+V), drop them on the window, or put them in the workspace's "
                               "survivors or assembly folder -- they show up on their own.");
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 6 * g_Scale));
        }
        DrawFolderTree(std::string(), rowH);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));   // close the manual cursor placement
        DrawZombieSection();
    }

    // Anywhere else in the list is the top level: dropped there, groups and
    // folders come out of their folders. (The smallest target under the mouse
    // wins, so the rows keep theirs.)
    {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        const ImRect all = w->InnerRect;
        if (ImGui::BeginDragDropTargetCustom(all, ImGui::GetID("##rootdrop")))
        {
            if (const ImGuiPayload* pay = ImGui::AcceptDragDropPayload("CW_ROW"))
                QueueMove(ItemsForAction(*(const Ref*)pay->Data), std::string());
            ImGui::EndDragDropTarget();
        }
        // and files dropped there land at the top (the rows and the zombie
        // section added their own zones first; the first match wins)
        AddDropZone(all.Min, all.Max, std::string(), false);
        // a right-click on empty space: what can be made here
        const bool emptyHere = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(all.Min, all.Max) &&
                               !ImGui::IsAnyItemHovered();
        if (emptyHere && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##wsempty");
        if (emptyHere && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
            S.selection.clear();
        if (ImGui::BeginPopup("##wsempty"))
        {
            if (ImGui::MenuItem("New group", "Ctrl+N")) QueueWsOp(WsOp::NewGroup);
            if (ImGui::MenuItem("New folder")) QueueWsOp(WsOp::NewFolder);
            if (ImGui::MenuItem("New zombie")) QueueWsOp(WsOp::NewZombie);
            ImGui::Separator();
            if (ImGui::MenuItem("Paste files", "Ctrl+V")) QueueWsOp(WsOp::Paste);
            if (ImGui::MenuItem("Select all", "Ctrl+A")) SelectAllRows();
            ImGui::EndPopup();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::SetCursorScreenPos(ImVec2(origin.x + width, origin.y));
}
// -----------------------------------------------------------------------------
// Docked panels
// -----------------------------------------------------------------------------
static void DrawEditorPanel()
{
    AppState& S = g_App;
    if (g_FocusEditorFrames > 0)
    {
        g_FocusEditorFrames--;
        ImGui::SetNextWindowFocus();   // make Editor the active tab after a layout build
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::Begin("Editor", NULL, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if (open)
    {
        // breadcrumb strip
        const float crumbH = 28.0f * g_Scale;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        dl->AddRectFilled(o, ImVec2(o.x + w, o.y + crumbH), HexU(Col::Bg1));
        dl->AddLine(ImVec2(o.x, o.y + crumbH - 1), ImVec2(o.x + w, o.y + crumbH - 1), HexU(Col::Line));

        Survivor* sv = SurvivorAt(S.open);
        const float ty = o.y + (crumbH - ImGui::GetTextLineHeight()) * 0.5f;
        if (sv && sv->present)
        {
            const std::string owner = OwnerNameOf(S.open);
            float x = o.x + 12 * g_Scale;
            dl->AddCircleFilled(ImVec2(x + 4 * g_Scale, o.y + crumbH * 0.5f), 4 * g_Scale,
                                HexU(OwnerTintOf(S.open)));
            x += 16 * g_Scale;
            dl->AddText(ImVec2(x, ty), HexU(Col::TextDim), owner.c_str());
            x += ImGui::CalcTextSize(owner.c_str()).x + 7 * g_Scale;
            dl->AddText(ImVec2(x, ty), HexU(Col::TextDim, 0.55f), "/");
            x += ImGui::CalcTextSize("/").x + 7 * g_Scale;
            // "{group} 1" / "{group} 2" is the survivor's identity; the file it
            // came from is secondary, so it trails in the dim colour.
            char who[96];
            if (IsZombieRef(S.open)) snprintf(who, sizeof(who), "%s", sv->name.c_str());
            else                     snprintf(who, sizeof(who), "%s %d", owner.c_str(), S.open.slot + 1);
            dl->AddText(ImVec2(x, ty), HexU(Col::Text), who);
            x += ImGui::CalcTextSize(who).x + 8 * g_Scale;

            if (sv->dirty)
                dl->AddCircleFilled(ImVec2(x, o.y + crumbH * 0.5f), 3 * g_Scale, HexU(Col::Warn));
            // assembled size and error count, right-aligned -- what the removed
            // editor toolbar used to carry
            {
                int nerr = 0;
                for (const cw::AsmDiag& d : sv->asmDiags) if (d.error) nerr++;
                char info[64];
                if (nerr) snprintf(info, sizeof(info), "%d bytes   %d error%s",
                                   (int)sv->bin.size(), nerr, nerr == 1 ? "" : "s");
                else      snprintf(info, sizeof(info), "%d bytes", (int)sv->bin.size());
                const ImVec2 isz = ImGui::CalcTextSize(info);
                const float ix = o.x + w - isz.x - (OwnerEnabledOf(S.open) && sv->enabled
                                                    ? 12 * g_Scale
                                                    : ImGui::CalcTextSize("NOT ACTIVE").x + 36 * g_Scale);
                if (ix > x) dl->AddText(ImVec2(ix, ty), HexU(nerr ? Col::Danger : Col::TextDim), info);
            }

            if (!OwnerEnabledOf(S.open) || !sv->enabled)
            {
                const char* tag = "NOT ACTIVE";
                const ImVec2 tsz = ImGui::CalcTextSize(tag);
                dl->AddRectFilled(ImVec2(o.x + w - tsz.x - 24 * g_Scale, o.y + (crumbH - tsz.y) * 0.5f - 3 * g_Scale),
                                  ImVec2(o.x + w - 12 * g_Scale, o.y + (crumbH + tsz.y) * 0.5f + 3 * g_Scale),
                                  HexU(Col::Danger, 0.22f), 4 * g_Scale);
                dl->AddText(ImVec2(o.x + w - tsz.x - 18 * g_Scale, ty), HexU(Col::Danger), tag);
            }
        }
        else
        {
            dl->AddText(ImVec2(o.x + 12 * g_Scale, ty), HexU(Col::TextDim), "No survivor open");
        }
        ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + crumbH));

        Survivor* open = SurvivorAt(S.open);

        // --- gutter + code, sharing one scroll ------------------------------
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        if (S.fontMono) ImGui::PushFont(S.fontMono, S.fontEditorSize * g_Scale);
        // TextEditor pushes ItemSpacing(0,0) before it works out its own row
        // pitch, so its rows are exactly one font-height tall. Stepping the
        // gutter by GetTextLineHeightWithSpacing() added this window's
        // ItemSpacing.y to every row and the two drifted apart down the file.
        const float lineH = ImGui::GetTextLineHeight();
        const int   nLines = ImMax(1, S.editor.GetTotalLines());
        // Run both columns to the bottom of the panel even when the source is
        // short, so the gutter does not stop in mid-air above empty space.
        const float contentH = ImMax(nLines * lineH + 8.0f * g_Scale, avail.y);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg2));
        ImGui::BeginChild("##codearea", avail, false, ImGuiWindowFlags_HorizontalScrollbar);
        float gutW = 0.0f;
        if (S.showAsmGutter && open && open->present)
        {
            gutW = DrawByteGutter(*open, nLines, lineH, contentH);
            ImGui::SameLine(0, 0);
        }
        // TextEditor opens its own child, which would otherwise inherit the
        // app's WindowPadding and push the first text row below the gutter's
        // first row. Zero the vertical half so the two line up exactly; the row
        // pitch already matches (mLineSpacing is 1, so mCharAdvance.y ==
        // GetTextLineHeightWithSpacing(), which is what the gutter steps by).
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * g_Scale, 0.0f));
        S.editor.Render("##texteditor",
                        ImVec2(ImMax(120.0f, avail.x - gutW - 2.0f), contentH), false);
        ImGui::PopStyleVar();

        // The editor draws its whole text, so this child is what scrolls: keep
        // the caret inside it, and while a selection is being dragged let the
        // edges pull the view along so the selection can run past the bottom.
        {
            const float viewH = ImGui::GetWindowHeight();
            const float caretY = S.editor.GetCursorPosition().mLine * lineH;
            if (S.editor.IsCursorPositionChanged())
            {
                if (caretY < ImGui::GetScrollY())
                    ImGui::SetScrollY(caretY);
                else if (caretY + lineH > ImGui::GetScrollY() + viewH)
                    ImGui::SetScrollY(caretY + lineH - viewH);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !S.editor.IsReadOnly())
            {
                const ImVec2 mn = ImGui::GetWindowPos();
                const float my = ImGui::GetIO().MousePos.y;
                const float edge = 18.0f * g_Scale;
                const float sp = 12.0f * g_Scale;
                if (my > mn.x * 0.0f + mn.y && my < mn.y + edge)
                    ImGui::SetScrollY(ImMax(0.0f, ImGui::GetScrollY() - sp));
                else if (my > mn.y + viewH - edge && my < mn.y + viewH)
                    ImGui::SetScrollY(ImMin(ImGui::GetScrollMaxY(), ImGui::GetScrollY() + sp));
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (S.fontMono) ImGui::PopFont();

        // A double-click in the gutter asked for the listing to be re-read from
        // one byte onwards; do it now the gutter has finished with the survivor.
        if (g_ReframeAt >= 0)
        {
            const int at = g_ReframeAt;
            g_ReframeAt = -1;
            if (open && open->present && at < (int)open->bin.size())
            {
                const std::vector<unsigned char> bytes = open->bin;
                g_ReframeUndo.push_back(open->code);   // Ctrl+Z puts this back
                g_ReframeRedo.clear();
                ListSurvivor(*open, bytes, at);
                open->dirty = true;
                NoteEdit();
                S.editor.SetText(open->code);
                PushEditorMarkers(*open);
                char m[80];
                snprintf(m, sizeof(m), "Re-read the code from %04X", (unsigned)at);
                S.status = m;
            }
        }

        // --- realtime assembly ----------------------------------------------
        if (open && open->present && S.editor.IsTextChanged())
        {
            FlushEditorToModel();
            // a ";$" line's db follows its instruction text as it is typed
            if (SyncAltLinesLive(*open, S.editor)) FlushEditorToModel();
        }
        // The markers follow the survivor's diagnostics every frame: they were
        // only pushed on a keystroke, so a file that was just opened (or
        // re-assembled from elsewhere) showed none of its errors.
        if (open && open->present) PushEditorMarkers(*open);
    }
    ImGui::End();
}

// Colour for a session group index: match by name back to a workspace player so
// renames / custom tints carry through; zombies (gi < 0) get a fixed grey-green.
static unsigned GroupColor(int gi)
{
    if (gi < 0 || gi >= g_App.game.groupCount()) return 0x8A9BA8;
    const int p = WarGroupPlayer(gi);
    if (p >= 0) return g_App.players[(size_t)p].tint;
    return kPlayerTints[(size_t)gi % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
}

// warrior -> display colour (group tint, slot 1 a touch lighter)
// -----------------------------------------------------------------------------
// Byte-pattern breakpoints
//
// The engine already flags its own BRK (EB 00) as CF_BRK. On top of that the
// user can name byte patterns in the options; a cycle breaks when the
// instruction it ran matches one of them.
// -----------------------------------------------------------------------------
static std::vector<std::vector<unsigned char> > g_BreakPats;
static std::string g_BreakPatsSrc = "\x01";   // never equal to a real setting

static void RefreshBreakPatterns()
{
    if (g_BreakPatsSrc == g_App.breakPatterns) return;
    g_BreakPatsSrc = g_App.breakPatterns;
    g_BreakPats.clear();

    std::vector<unsigned char> cur;
    int nib = 0, val = 0;
    const char* p = g_App.breakPatterns;
    for (;; p++)
    {
        const char ch = *p;
        int d = -1;
        if (ch >= '0' && ch <= '9') d = ch - '0';
        else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10;

        if (d >= 0) { val = val * 16 + d; if (++nib == 2) { cur.push_back((unsigned char)val); nib = 0; val = 0; } }
        else
        {
            if (nib == 1) { cur.push_back((unsigned char)val); nib = 0; val = 0; }
            if (ch == ',' || ch == ';' || ch == '\0')
            {
                if (!cur.empty()) g_BreakPats.push_back(cur);
                cur.clear();
            }
        }
        if (ch == '\0') break;
    }
}

static bool CycleIsBreak(const cw::Replay& R, cw::u64 c)
{
    if (c >= R.cycleCount()) return false;
    if (R.cycles[(size_t)c].flags & cw::CF_BRK) return true;
    if (g_BreakPats.empty()) return false;

    const cw::u8* ops = R.opsOf(c);
    if (!ops) return false;
    // Only the bytes the instruction actually consumed count; the rest of the
    // six recorded bytes are whatever followed it in memory.
    int len = cw::disasm(ops, cw::kOpBytes).len;
    if (len <= 0 || len > cw::kOpBytes) len = cw::kOpBytes;

    for (const std::vector<unsigned char>& pat : g_BreakPats)
    {
        const int n = (int)pat.size();
        if (n > len) continue;
        if (g_App.breakStartsWith)
        {
            if (memcmp(ops, pat.data(), (size_t)n) == 0) return true;
        }
        else
        {
            for (int i = 0; i + n <= len; i++)
                if (memcmp(ops + i, pat.data(), (size_t)n) == 0) return true;
        }
    }
    return false;
}

static ImU32 WarriorColorSlow(int wi)
{
    // Zombies belong to no scored group, so they carry no group colour -- their
    // shade comes from the row that owns them in the groups bar.
    if (g_App.game.warriorZombie(wi))
    {
        const std::string& nm = g_App.game.replay().warriorName[(size_t)wi];
        for (const Zombie& z : g_App.zombies)
            if (z.name == nm) return ImGui::ColorConvertFloat4ToU32(Hex(z.tint, 1.0f));
        return ImGui::ColorConvertFloat4ToU32(Hex(Col::Warn, 1.0f));
    }

    const int gi = g_App.game.warriorGroup(wi);
    const int sl = g_App.game.warriorSlot(wi);
    unsigned base = GroupColor(gi);
    const int p = WarGroupPlayer(gi);
    if (p >= 0 && g_App.players[(size_t)p].twoTone)
    {
        const Player& pl = g_App.players[(size_t)p];
        return ImGui::ColorConvertFloat4ToU32(Hex((sl == 1) ? pl.tint2 : pl.tint, 1.0f));
    }
    return ImGui::ColorConvertFloat4ToU32(Hex(SlotTint(base, sl), 1.0f));
}

// Finding a warrior's colour means matching its group by name against every
// survivor in the workspace, and the arena asks once per owned byte -- with a
// war that has written over the whole arena and a workspace of hundreds of
// survivors, that was millions of string compares a frame. So each warrior's
// colour is worked out once a frame and kept.
static ImU32 WarriorColor(int wi, float alpha)
{
    static int cacheFrame = -1;
    static cw::u64 cacheGen = ~0ull;
    static std::vector<ImU32> cache;                    // 0 = not worked out yet
    if (cacheFrame != g_FrameNo || cacheGen != g_App.game.generation())
    {
        cacheFrame = g_FrameNo;
        cacheGen = g_App.game.generation();
        cache.assign(cache.size(), 0u);
    }
    if (wi < 0) return ImGui::ColorConvertFloat4ToU32(Hex(Col::Warn, alpha));
    if ((size_t)wi >= cache.size()) cache.resize((size_t)wi + 1, 0u);
    ImU32& c = cache[(size_t)wi];
    if (c == 0u) c = WarriorColorSlow(wi) | IM_COL32_A_MASK;
    const int a = (int)(ImSaturate(alpha) * 255.0f + 0.5f);
    return (c & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
}


// -----------------------------------------------------------------------------
// Edit tool
//
// With it on, double-clicking a byte in the arena, the stack or the shared page
// -- or a register in the survivor panel -- turns that byte or register into a
// text field where it stands. Enter or clicking away keeps what was typed, Esc
// drops it. The change is kept as a Poke against the cycle it was made at; the
// war is then played out again from the seed with that poke applied, so
// everything after the edit follows from it. The re-record is deferred until
// the next forward move, so a run of edits costs one recomputation rather than
// one each.
// -----------------------------------------------------------------------------
struct EditTarget
{
    bool     open = false;
    bool     isReg = false;
    cw::s32  linear = 0;
    int      warrior = 0, reg = 0;
    unsigned cur = 0;
    char     what[72] = "";
    char     buf[16] = "";
    ImGuiID  win = 0;              // the panel whose byte or register it is
    bool     focus = false;        // the field takes the keyboard on its first frame
    int      openedFrame = -1;
    int      placedFrame = -1;     // the last frame the panel laid the target out
    ImVec2   mn, mx;               // and where it put it
    int      drawnFrame = -1;
};
static EditTarget g_Edit;

static ImGuiID EditPanelId() { return ImGui::GetCurrentWindow()->ID; }

static void EditBegin(const char* say)
{
    g_Edit.open = true;
    g_Edit.win = EditPanelId();
    g_Edit.focus = true;
    g_Edit.openedFrame = g_FrameNo;
    char m[128];
    snprintf(m, sizeof(m), "%s: type the new value -- Enter or click away keeps it, Esc drops it", say);
    g_App.status = m;
}

static void RequestByteEdit(cw::s32 linear, unsigned value, const char* where)
{
    if (!g_App.editMode || !g_App.game.ready()) return;
    g_Edit = EditTarget();
    g_Edit.linear = linear;
    g_Edit.cur = value & 0xFF;
    snprintf(g_Edit.what, sizeof(g_Edit.what), "%s byte %05X", where, (unsigned)linear);
    snprintf(g_Edit.buf, sizeof(g_Edit.buf), "%02X", value & 0xFF);
    EditBegin(g_Edit.what);
}

static void RequestRegEdit(int warrior, int reg, const char* name, unsigned value)
{
    if (!g_App.editMode || !g_App.game.ready()) return;
    g_Edit = EditTarget();
    g_Edit.isReg = true;
    g_Edit.warrior = warrior;
    g_Edit.reg = reg;
    g_Edit.cur = value & 0xFFFF;
    snprintf(g_Edit.what, sizeof(g_Edit.what), "register %s", name);
    snprintf(g_Edit.buf, sizeof(g_Edit.buf), "%04X", value & 0xFFFF);
    EditBegin(g_Edit.what);
}

// Whether the edit is about this byte or register, in the panel being drawn.
static bool EditIsByte(cw::s32 linear)
{
    return g_Edit.open && !g_Edit.isReg && g_Edit.linear == linear && g_Edit.win == EditPanelId();
}
static bool EditIsReg(int warrior, int reg)
{
    return g_Edit.open && g_Edit.isReg && g_Edit.warrior == warrior && g_Edit.reg == reg &&
           g_Edit.win == EditPanelId();
}

// Where the panel put the target this frame; the field goes over it.
static void EditPlace(const ImVec2& mn, const ImVec2& mx)
{
    g_Edit.mn = mn;
    g_Edit.mx = mx;
    g_Edit.placedFrame = g_FrameNo;
}

// Put one edit on the list. A second edit of the same register at the same
// moment replaces the first rather than queueing behind it, so flipping a flag
// twice is not two pokes fighting over one word.
static void StagePoke(const cw::Poke& p, const char* what, const char* valueText)
{
    bool replaced = false;
    if (p.isReg)
        for (cw::Poke& q : g_App.pokes)
            if (q.isReg && q.cycle == p.cycle && q.warrior == p.warrior && q.reg == p.reg)
            { q = p; replaced = true; break; }
    if (!replaced) g_App.pokes.push_back(p);
    g_App.pokesPending = true;
    g_App.pokeRound = g_App.game.round();
    char m[160];
    snprintf(m, sizeof(m), "%s -> %s at round %d  (recomputes on the next move)",
             what, valueText, g_App.pokeRound);
    g_App.status = m;
}

// A register edit with no dialog in between -- what a flag chip does.
static void StageRegEdit(int warrior, int reg, const char* name, unsigned value)
{
    if (!g_App.editMode || !g_App.game.ready()) return;
    cw::Poke p;
    p.cycle = g_App.game.cycle();
    p.isReg = true;
    p.warrior = warrior;
    p.reg = reg;
    p.regValue = (cw::u16)value;
    char what[72], val[16];
    snprintf(what, sizeof(what), "register %s", name);
    snprintf(val, sizeof(val), "%04X", value & 0xFFFF);
    StagePoke(p, what, val);
}

// The value a register has been given by an edit that is staged but not yet
// played out. The recording still holds the old one until the war is recomputed,
// so the inspector asks here first -- otherwise a second flag flipped before the
// recompute would be worked out from the stale word and undo the first.
static bool PendingRegValue(int warrior, int reg, unsigned& value)
{
    if (!g_App.pokesPending || !g_App.game.ready()) return false;
    const cw::u64 c = g_App.game.cycle();
    for (size_t i = g_App.pokes.size(); i-- > 0; )
    {
        const cw::Poke& p = g_App.pokes[i];
        if (p.isReg && p.cycle == c && p.warrior == warrior && p.reg == reg)
        { value = p.regValue; return true; }
    }
    return false;
}

// What was typed becomes a poke -- unless it is what was there already, which
// would only cost a recomputation for nothing.
static void EditCommit()
{
    g_Edit.open = false;
    if (!g_Edit.buf[0] || !g_App.game.ready()) { g_App.status = "Edit dropped"; return; }
    const unsigned v = (unsigned)strtoul(g_Edit.buf, NULL, 16) & (g_Edit.isReg ? 0xFFFFu : 0xFFu);
    if (v == g_Edit.cur) { g_App.status = "Unchanged"; return; }
    cw::Poke p;
    p.cycle = g_App.game.cycle();
    p.isReg = g_Edit.isReg;
    if (g_Edit.isReg)
    {
        p.warrior = g_Edit.warrior;
        p.reg = g_Edit.reg;
        p.regValue = (cw::u16)v;
    }
    else
    {
        p.linear = g_Edit.linear;
        p.value = (cw::u8)v;
    }
    char val[8];
    snprintf(val, sizeof(val), g_Edit.isReg ? "%04X" : "%02X", v);
    StagePoke(p, g_Edit.what, val);
}

// The field, over the byte or register being edited. Each panel that can hold
// one calls this once it has laid its cells out (EditPlace), still inside its
// own window; `fontSize` 0 keeps the size it is drawing at.
static void EditField(float fontSize = 0.0f)
{
    if (!g_Edit.open || g_Edit.win != EditPanelId() || g_Edit.placedFrame != g_FrameNo) return;
    ImGui::PushFont(g_App.fontMono, fontSize > 0.0f ? fontSize : ImGui::GetFontSize());
    const ImVec2 pad(3.0f * g_Scale, 1.0f * g_Scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, pad);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f * g_Scale);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Hex(Col::Bg0));
    ImGui::PushStyleColor(ImGuiCol_Border, Hex(Col::Accent2));
    const ImVec2 mn = g_Edit.mn, mx = g_Edit.mx;
    // as wide as the cell, and never too narrow for the digits and the caret
    const float digits = ImGui::CalcTextSize(g_Edit.isReg ? "FFFF" : "FF").x;
    const float w = ImMax(digits + pad.x * 2.0f + 3.0f * g_Scale, mx.x - mn.x);
    const float h = ImGui::GetFrameHeight();
    ImGui::SetCursorScreenPos(ImVec2(ImFloor((mn.x + mx.x - w) * 0.5f), ImFloor((mn.y + mx.y - h) * 0.5f)));
    ImGui::SetNextItemWidth(w);
    if (g_Edit.focus) { ImGui::SetKeyboardFocusHere(); g_Edit.focus = false; }
    const bool enter = ImGui::InputText("##inplaceedit", g_Edit.buf, g_Edit.isReg ? 5 : 3,
                                        ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase |
                                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    const bool escaped = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    const bool leftIt = ImGui::IsItemDeactivated();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PopFont();
    g_Edit.drawnFrame = g_FrameNo;
    if (escaped) { g_Edit.open = false; g_App.status = "Edit dropped"; }
    else if (enter || leftIt) EditCommit();
}

// An edit whose byte or register is no longer on screen -- scrolled away, its
// panel closed, the edit tool switched off -- is dropped rather than left open
// where nobody can see it. Called once a frame, after the panels.
static void EditFieldSweep()
{
    if (!g_Edit.open) return;
    if (!g_App.editMode || !g_App.game.ready()) { g_Edit.open = false; return; }
    if (ImMax(g_Edit.drawnFrame, g_Edit.openedFrame) < g_FrameNo - 1) g_Edit.open = false;
}

// ---------------------------------------------------------------------------
// Pointer rings -- shared by the arena, the stack, the shared page and the code
//
// The segment:register pairs that can be marked, in the order the options list
// them. Bit i of AppState::hlMask turns pair i on, in every panel at once.
// ---------------------------------------------------------------------------
struct PointerPair { const char* name; int seg; int reg; };
static const PointerPair kPointerPairs[] = {
    { "CS:IP", cw::PR_CS, cw::PR_IP },
    { "DS:AX", cw::PR_DS, cw::PR_AX }, { "DS:BX", cw::PR_DS, cw::PR_BX },
    { "DS:CX", cw::PR_DS, cw::PR_CX }, { "DS:DX", cw::PR_DS, cw::PR_DX },
    { "DS:SI", cw::PR_DS, cw::PR_SI }, { "DS:DI", cw::PR_DS, cw::PR_DI },
    { "ES:SI", cw::PR_ES, cw::PR_SI }, { "ES:DI", cw::PR_ES, cw::PR_DI },
    { "SS:BP", cw::PR_SS, cw::PR_BP }, { "SS:SP", cw::PR_SS, cw::PR_SP },
};
static const int kPointerPairCount = (int)(sizeof(kPointerPairs) / sizeof(kPointerPairs[0]));

// One enabled pair of a warrior, as the linear address it points at. CS:IP is
// left out where IP already has a marker of its own (the arena, the code).
struct PtrHit { cw::s32 lin; const char* name; };
static std::vector<PtrHit> PointerHits(const cw::CpuState& st, bool withIp)
{
    std::vector<PtrHit> out;
    if (!g_App.hlMask) return out;
    cw::CpuState s = st;
    for (int i = 0; i < kPointerPairCount; i++)
    {
        if (!(g_App.hlMask & (1u << i))) continue;
        if (i == 0 && !withIp) continue;
        const PointerPair& pp = kPointerPairs[i];
        out.push_back({ cw::RMAddr(*s.slot(pp.seg), *s.slot(pp.reg)).linear, pp.name });
    }
    return out;
}

// Where a linear address falls in a 64K window that starts at segment `seg`:
// its offset there, or -1 when it is outside the window.
static int OffsetInSegment(cw::s32 lin, cw::s16 seg)
{
    cw::s32 d = lin - cw::RMAddr(seg, 0).linear;
    if (d < 0) d += 0x100000;            // 20-bit wrap
    return (d >= 0 && d < 0x10000) ? (int)d : -1;
}

// Black or white, whichever stands out against `c`: the hairline that keeps a
// coloured ring visible against the cell inside it. A dark ring gets white.
static ImU32 HairlineFor(ImU32 c, float alpha = 0.9f)
{
    const ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    const float lum = 0.299f * v.x + 0.587f * v.y + 0.114f * v.z;
    return lum < 0.5f ? IM_COL32(255, 255, 255, (int)(alpha * 255)) : IM_COL32(11, 14, 19, (int)(alpha * 255));
}

// The ring itself: the pointer colour, with a hairline of contrast inside it.
static void PointerRing(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx, float rounding, float thick)
{
    const ImU32 ring = HexU(g_App.hlColor);
    dl->AddRect(mn, mx, ring, rounding, 0, thick);
    const float in = thick * 0.5f + 0.5f;
    dl->AddRect(ImVec2(mn.x + in, mn.y + in), ImVec2(mx.x - in, mx.y - in),
                HairlineFor(ring, 0.6f), ImMax(0.0f, rounding - 1.0f), 0, 1.0f);
}

// A pointer's name in a list (stack, shared page, code): it sits over the
// row's index, and a click moves it over the row's content instead -- one or
// the other is always covered, so the click is how to read what is under it.
// Several on one row line up side by side. Returns the tag's width.
static std::map<std::string, bool> g_TagOnContent;
static float PointerListTag(const char* panel, const char* name, const ImVec2& indexMn, float rowH,
                            float contentX, int k)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const std::string key = std::string(panel) + "|" + name;
    bool& onContent = g_TagOnContent[key];
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.8f);
    const ImVec2 ts = ImGui::CalcTextSize(name);
    const float w = ts.x + 8 * g_Scale, h = ImMin(rowH - 1.0f, ts.y + 3 * g_Scale);
    const float x = (onContent ? contentX : indexMn.x - 2 * g_Scale) + k * (w + 2 * g_Scale);
    const ImVec2 mn(x, indexMn.y + (rowH - h) * 0.5f - 1.0f), mx(x + w, mn.y + h);
    const ImU32 bg = HexU(g_App.hlColor);
    dl->AddRectFilled(mn, mx, bg, h * 0.5f);
    dl->AddText(ImVec2(mn.x + 4 * g_Scale, mn.y + (h - ts.y) * 0.5f), HairlineFor(bg, 1.0f), name);
    ImGui::PopFont();
    if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(mn, mx))
    {
        ImGui::SetTooltip("%s\nClick: move it over the %s", name, onContent ? "index" : "content");
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) onContent = !onContent;
    }
    return w;
}

#include "gui/gui_inspector.inl"   // registers / flags / code / stack / shared panels

// Row / column offsets pinned to the edges of the arena view, so a cell can be
// placed without counting. The stride grows as you zoom out so the labels never
// crowd each other.
// The grid is inset by exactly these, so the rulers sit beside the arena rather
// than on top of it and cell 0,0 can always be scrolled clear of them.
static float ArenaRulerW() { return ImGui::CalcTextSize("00").x + 12 * g_Scale; }
static float ArenaRulerH() { return ImGui::GetTextLineHeight() + 5 * g_Scale; }
// Wide enough for a pointer tag such as "DS:BX" in the left strip.
static float ArenaTagW()
{
    if (!g_App.hlMask) return 0.0f;
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.8f);
    const float w = ImGui::CalcTextSize("SS:SP").x + 12 * g_Scale;
    ImGui::PopFont();
    return w;
}

static void ArenaRulers(const ImVec2& p0, float cell, int N, float lw, float lh)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wpos = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();

    // A byte's offset is its row and its column: the row (the high byte) down
    // the side, the column (the low byte) along the top -- "12" and "34" for
    // 0x1234, each written the same way.
    int stride = 1;
    while (stride < N && stride * cell < 30.0f * g_Scale) stride *= 2;

    dl->PushClipRect(wpos, ImVec2(wpos.x + wsz.x, wpos.y + wsz.y), true);
    // opaque: once the arena is panned its cells run underneath these strips
    dl->AddRectFilled(wpos, ImVec2(wpos.x + lw, wpos.y + wsz.y), HexU(Col::Bg0));
    dl->AddRectFilled(ImVec2(wpos.x + lw, wpos.y), ImVec2(wpos.x + wsz.x, wpos.y + lh), HexU(Col::Bg0));

    const float th = ImGui::GetTextLineHeight();
    char t[8];
    dl->PushClipRect(ImVec2(wpos.x, wpos.y + lh), ImVec2(wpos.x + lw, wpos.y + wsz.y), true);
    for (int y = 0; y < N; y += stride)
    {
        const float sy = p0.y + y * cell + (cell - th) * 0.5f;
        if (sy + th < wpos.y + lh || sy > wpos.y + wsz.y) continue;
        snprintf(t, sizeof(t), "%02X", (unsigned)y);
        const float tw = ImGui::CalcTextSize(t).x;
        dl->AddText(ImVec2(wpos.x + lw - tw - 6 * g_Scale, sy), HexU(Col::TextDim, 0.75f), t);
    }
    dl->PopClipRect();

    dl->PushClipRect(ImVec2(wpos.x + lw, wpos.y), ImVec2(wpos.x + wsz.x, wpos.y + lh), true);
    for (int x = 0; x < N; x += stride)
    {
        const float sx = p0.x + x * cell;
        if (sx + cell + 24 * g_Scale < wpos.x + lw || sx > wpos.x + wsz.x) continue;
        snprintf(t, sizeof(t), "%02X", (unsigned)x);
        // over the middle of its column when the column is wide enough to hold
        // it, otherwise starting at the column's left line
        const float tw = ImGui::CalcTextSize(t).x;
        const float tx = (cell >= tw + 4 * g_Scale) ? sx + (cell - tw) * 0.5f : sx + 2 * g_Scale;
        dl->AddText(ImVec2(tx, wpos.y + 2 * g_Scale), HexU(Col::TextDim, 0.75f), t);
    }
    dl->PopClipRect();

    dl->AddLine(ImVec2(wpos.x + lw, wpos.y), ImVec2(wpos.x + lw, wpos.y + wsz.y), HexU(Col::Line, 0.9f));
    dl->AddLine(ImVec2(wpos.x, wpos.y + lh), ImVec2(wpos.x + wsz.x, wpos.y + lh), HexU(Col::Line, 0.9f));
    dl->PopClipRect();
}

// The grid at this zoom. Below kGridOn px a cell has no lines at all; from
// there on the lines have their full width -- never thinner than a pixel, since
// sub-pixel lines shimmer as the view moves -- and it is their opacity that
// eases in, from transparent at kGridOn to solid at kGridFull.
static const float kGridOn = 9.0f, kGridFull = 16.0f;
static float ArenaLineScale(float cell)        // thickness setting, or 0 below the threshold
{
    return (cell >= kGridOn) ? ImClamp(g_App.arenaGridWidth, 0.0f, 3.0f) : 0.0f;
}
static float ArenaLineFade(float cell)         // 0 transparent .. 1 solid
{
    float t = ImClamp((cell - kGridOn) / (kGridFull - kGridOn), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);          // smoothstep
}

// Where the inspected warrior's chosen pointers land this frame: arena offset
// and the pair's name. Filled by ArenaPointerRings, read by ArenaPointerTags.
struct PointerMark { int off; const char* name; };
static std::vector<PointerMark> g_PointerMarks;

// Rings the cells the inspected warrior's chosen pointers land on. Each pair
// is worked out through its own segment, so one whose segment has been thrown
// out of the arena simply has no cell. Nothing is written on the cells: the
// pair's name goes in the offset strips beside the arena (ArenaPointerTags).
static void ArenaPointerRings(cw::GuiSession& g, const ImVec2& p0, float cell, int N,
                              int x0, int y0, int x1, int y1)
{
    const AppState& S = g_App;
    g_PointerMarks.clear();
    if (!S.hlMask) return;

    int wi = S.inspectWarrior;
    if (wi < 0 || wi >= g.numWarriors()) wi = g.player().actingWarrior();
    if (wi < 0 || wi >= g.numWarriors()) return;

    cw::CpuState st = g.player().cpu(wi);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float thick    = ImClamp(cell * 0.13f, 1.6f, 3.0f);
    // the cells' own rounding (see DrawArenaGrid), grown by the ring's offset
    // so the ring runs parallel to the cell's corner
    const float gw       = ArenaLineScale(cell);
    const float cellRnd  = ImMin(cell * 0.10f * gw, cell * 0.30f);
    const float rounding = cellRnd > 0.0f ? cellRnd + thick * 0.5f + 0.5f : 0.0f;

    std::vector<int> ringed;
    // CS:IP is left out here: IP already has its ring of its own
    for (const PtrHit& h : PointerHits(st, false))
    {
        const cw::s32 lin = h.lin - cw::SNAP_LO;
        if (lin < 0 || lin >= N * N) continue;
        g_PointerMarks.push_back({ (int)lin, h.name });

        // one ring per cell, however many pairs share it
        if (std::find(ringed.begin(), ringed.end(), (int)lin) != ringed.end()) continue;
        ringed.push_back((int)lin);
        const int x = lin % N, y = lin / N;
        if (x < x0 - 1 || x > x1 || y < y0 - 1 || y > y1) continue;

        // the ring sits just outside the cell, coloured, with a hairline of
        // contrast inside it so it reads on any cell colour
        const ImVec2 a(p0.x + x * cell, p0.y + y * cell);
        const float  o = thick * 0.5f + 0.5f;
        PointerRing(dl, ImVec2(a.x - o, a.y - o), ImVec2(a.x + cell + o, a.y + cell + o), rounding, thick);
    }
}

// The pairs' names, in the offset strips: in the top strip over the ringed
// cell's column and in the left strip beside its row, so they never sit on top
// of the arena itself.
//
// Each pair has a label of its own. Where labels land on top of one another
// (two pairs on one column, or neighbouring columns at a small zoom), clicking
// the one in front slides it aside to the nearest free spot -- its pointer
// stays on its column, a leader line runs to it -- which uncovers the next;
// clicking that one moves it too, and so on. A moved label that would no longer
// cover anything at home goes back by itself; clicking it sends it back early.
struct TagSlide { float cur = 0.0f, target = 0.0f; };
static std::map<std::string, TagSlide> g_TagSlides;

struct StripTag {
    std::string key;     // "T:DS:SI" / "L:DS:SI"
    const char* name;
    float anchor;        // the column's centre x (top) or the row's centre y (left)
    ImVec2 homeMn, size;
};

// Lays out, animates, draws and handles clicks for one strip's labels.
// `along` is 0 for the top strip (labels slide in x), 1 for the left (in y).
static void DrawStripTags(std::vector<StripTag>& tags, int along, const ImVec2& clipMn, const ImVec2& clipMx,
                          float edge)
{
    if (tags.empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg = HexU(g_App.hlColor), ink = HairlineFor(bg, 1.0f);
    const float dt = ImGui::GetIO().DeltaTime;
    const float lo = along == 0 ? clipMn.x : clipMn.y, hi = along == 0 ? clipMx.x : clipMx.y;

    auto rectAt = [&](const StripTag& t, float off, ImVec2& mn, ImVec2& mx) {
        mn = t.homeMn;
        if (along == 0) mn.x += off; else mn.y += off;
        mx = ImVec2(mn.x + t.size.x, mn.y + t.size.y);
    };
    auto overlaps = [](const ImVec2& a0, const ImVec2& a1, const ImVec2& b0, const ImVec2& b1) {
        return a0.x < b1.x - 0.5f && b0.x < a1.x - 0.5f && a0.y < b1.y - 0.5f && b0.y < a1.y - 0.5f;
    };

    // where everything is this frame
    std::vector<ImVec2> mn(tags.size()), mx(tags.size());
    for (size_t i = 0; i < tags.size(); i++)
    {
        TagSlide& s = g_TagSlides[tags[i].key];
        s.cur += (s.target - s.cur) * (1.0f - expf(-dt * 14.0f));
        if (fabsf(s.target - s.cur) < 0.3f) s.cur = s.target;
        rectAt(tags[i], s.cur, mn[i], mx[i]);
    }
    auto coversAny = [&](size_t i, const ImVec2& a0, const ImVec2& a1) {
        for (size_t j = 0; j < tags.size(); j++)
            if (j != i && overlaps(a0, a1, mn[j], mx[j])) return true;
        return false;
    };

    // a moved label that would sit clear at home goes back
    for (size_t i = 0; i < tags.size(); i++)
    {
        TagSlide& s = g_TagSlides[tags[i].key];
        if (s.target == 0.0f) continue;
        ImVec2 h0, h1; rectAt(tags[i], 0.0f, h0, h1);
        if (!coversAny(i, h0, h1)) s.target = 0.0f;
    }

    // a click goes to the label drawn last under the mouse -- the one in front
    int hot = -1;
    if (ImGui::IsWindowHovered())
        for (size_t i = tags.size(); i-- > 0; )
            if (ImGui::IsMouseHoveringRect(mn[i], mx[i])) { hot = (int)i; break; }
    if (hot >= 0)
    {
        TagSlide& s = g_TagSlides[tags[(size_t)hot].key];
        std::string under;
        for (size_t j = 0; j < tags.size(); j++)
            if ((int)j != hot && overlaps(mn[(size_t)hot], mx[(size_t)hot], mn[j], mx[j]))
                under += std::string(under.empty() ? "" : ", ") + tags[j].name;
        if (!under.empty())
            ImGui::SetTooltip("%s\ncovers %s - click to move it aside", tags[(size_t)hot].name, under.c_str());
        else if (s.target != 0.0f)
            ImGui::SetTooltip("%s\nclick to put it back", tags[(size_t)hot].name);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (!under.empty())
            {
                // the nearest offset, either way along the strip, where it covers
                // nothing and stays inside the strip
                const StripTag& t = tags[(size_t)hot];
                const float len = along == 0 ? t.size.x : t.size.y;
                const float home = along == 0 ? t.homeMn.x : t.homeMn.y;
                const float step = 2.0f * g_Scale;
                for (float d = step; d < (hi - lo) + len; d += step)
                {
                    bool done = false;
                    for (int sgn = 1; sgn >= -1 && !done; sgn -= 2)
                    {
                        const float off = s.cur + sgn * d;
                        if (home + off < lo || home + off + len > hi) continue;
                        ImVec2 a0, a1; rectAt(t, off, a0, a1);
                        if (!coversAny((size_t)hot, a0, a1)) { s.target = off; done = true; }
                    }
                    if (done) break;
                }
            }
            else s.target = 0.0f;
        }
    }

    dl->PushClipRect(clipMn, clipMx, true);
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.8f);
    for (size_t i = 0; i < tags.size(); i++)
    {
        const StripTag& t = tags[i];
        const ImVec2 a = mn[i], b = mx[i];
        const float r = along == 0 ? (b.y - a.y) * 0.5f : 3.0f * g_Scale;
        // the pointer always stays on the cell's column (or row); a label that
        // has been moved keeps a leader line back to it
        if (along == 0)
        {
            const float cx = t.anchor;
            const float lx = ImClamp(cx, a.x + r, b.x - r);
            if (fabsf(lx - cx) > 1.0f)
                dl->AddLine(ImVec2(lx, b.y), ImVec2(cx, edge - 3 * g_Scale), bg, 1.2f * g_Scale);
            dl->AddTriangleFilled(ImVec2(cx - 3 * g_Scale, edge - 4 * g_Scale), ImVec2(cx + 3 * g_Scale, edge - 4 * g_Scale),
                                  ImVec2(cx, edge), bg);
        }
        else
        {
            const float cy = t.anchor;
            const float ly = ImClamp(cy, a.y + r, b.y - r);
            if (fabsf(ly - cy) > 1.0f)
                dl->AddLine(ImVec2(b.x, ly), ImVec2(edge - 3 * g_Scale, cy), bg, 1.2f * g_Scale);
            dl->AddTriangleFilled(ImVec2(edge - 4 * g_Scale, cy - 3 * g_Scale), ImVec2(edge - 4 * g_Scale, cy + 3 * g_Scale),
                                  ImVec2(edge, cy), bg);
        }
        // a thin outline in the strip's colour keeps stacked labels apart
        dl->AddRectFilled(a, b, bg, r);
        dl->AddRect(a, b, HexU(Col::Bg1, 0.9f), r, 0, 1.0f);
        const ImVec2 ts = ImGui::CalcTextSize(t.name);
        dl->AddText(ImVec2(a.x + ImMax(1.0f, (b.x - a.x - ts.x) * 0.5f), a.y + (b.y - a.y - ts.y) * 0.5f), ink, t.name);
    }
    ImGui::PopFont();
    dl->PopClipRect();
}

static void ArenaPointerTags(const ImVec2& p0, float cell, int N, float lw, float lh)
{
    if (g_PointerMarks.empty() || lw <= 0.0f || lh <= 0.0f) return;
    const ImVec2 wpos = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();

    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.8f);
    const float th = ImGui::GetTextLineHeight();
    std::vector<StripTag> top, left;
    for (const PointerMark& m : g_PointerMarks)
    {
        const ImVec2 ts = ImGui::CalcTextSize(m.name);
        // top strip: centred over the column, kept inside the strip
        {
            const float cx = p0.x + (m.off % N + 0.5f) * cell;
            if (cx >= wpos.x + lw && cx <= wpos.x + wsz.x)
            {
                StripTag t;
                t.key = std::string("T:") + m.name;
                t.name = m.name;
                t.anchor = cx;
                t.size = ImVec2(ts.x + 8 * g_Scale, th + 2);
                t.homeMn = ImVec2(ImClamp(cx - t.size.x * 0.5f, wpos.x + lw + 1, wpos.x + wsz.x - t.size.x - 1),
                                  wpos.y + (lh - th) * 0.5f - 3);
                top.push_back(t);
            }
        }
        // left strip: beside the row
        {
            const float cy = p0.y + (m.off / N + 0.5f) * cell;
            if (cy >= wpos.y + lh && cy <= wpos.y + wsz.y)
            {
                StripTag t;
                t.key = std::string("L:") + m.name;
                t.name = m.name;
                t.anchor = cy;
                t.size = ImVec2(lw - 8 * g_Scale, th + 1);
                t.homeMn = ImVec2(wpos.x + 2 * g_Scale, cy - t.size.y * 0.5f);
                left.push_back(t);
            }
        }
    }
    ImGui::PopFont();

    DrawStripTags(top, 0, ImVec2(wpos.x + lw, wpos.y), ImVec2(wpos.x + wsz.x, wpos.y + lh), wpos.y + lh);
    DrawStripTags(left, 1, ImVec2(wpos.x, wpos.y + lh), ImVec2(wpos.x + lw, wpos.y + wsz.y), wpos.x + lw);
}

// An opaque mix of two colours. Arena cells are drawn solid -- a translucent
// cell lets whatever is behind it (the grid-line sheet, an unusual background)
// tint it, which is what made faded warriors look wrong.
static ImU32 MixOpaque(ImU32 a, ImU32 b, float t)
{
    const ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t,
                                                 x.z + (y.z - x.z) * t, 1.0f));
}

// ---------------------------------------------------------------------------
// Instruction banding around IP
//
// Every second instruction is shaded, so where one instruction ends and the
// next begins can be read straight off the arena. It covers the survivor's own
// stretch of code around IP, stopping where the cells stop being that
// survivor's or the reach runs out.
//
// Which instructions are shaded must not depend on where IP happens to be, or
// the pattern flips every time IP moves one instruction on. So instructions
// are counted from a fixed anchor -- the start of the survivor's block of code
// -- by decoding forward from it until the stream lands exactly on IP (x86
// cannot be decoded backwards). An instruction keeps its shade for as long as
// IP stays in the same stream of code.
// ---------------------------------------------------------------------------
static int ArenaInsnLen(const cw::u8* bytes, int off)
{
    cw::u8 b[cw::kOpBytes];
    for (int k = 0; k < cw::kOpBytes; k++) b[k] = bytes[(off + k) & 0xFFFF];
    const int len = cw::disasm(b, cw::kOpBytes).len;
    return ImClamp(len, 1, cw::kOpBytes);
}

static void BandInstructions(cw::GuiSession& g, const cw::u8* bytes, int ip, int reach,
                             std::vector<unsigned char>& mark, std::vector<int>& touched)
{
    const int owner = g.ownerAt(ip);
    auto inSection = [&](int off) { return g.ownerAt(off & 0xFFFF) == owner; };
    auto shade = [&](int start, int len, int lo, int hi) {
        for (int k = 0; k < len; k++)
        {
            const int o = start + k;
            if (o < lo || o > hi || !inSection(o)) continue;
            const int w = o & 0xFFFF;
            if (!mark[(size_t)w]) { mark[(size_t)w] = 1; touched.push_back(w); }
        }
    };

    // the part that is shaded: back and forth from IP while the cells are
    // still its owner's, up to the reach
    int lo = ip, hi = ip;
    while (ip - lo < reach && inSection(lo - 1)) lo--;
    while (hi - ip < reach && inSection(hi + 1)) hi++;

    // The anchor: where the survivor's block of code starts. A block can grow
    // very long as a warrior writes; past 4K back the anchor snaps to a fixed
    // 256-byte grid instead, which still does not move as IP steps along.
    static const int kAnchorMax = 4096;
    int base = ip;
    while (ip - base < kAnchorMax && inSection(base - 1)) base--;
    if (ip - base >= kAnchorMax)
    {
        base = ip - kAnchorMax;
        base -= ((base % 256) + 256) % 256;
    }

    // Decode forward from the anchor -- or the first byte or so after it, if
    // the anchor itself is mid-instruction -- until the stream lands on IP.
    // `starts` keeps every instruction boundary on the way, numbered from the
    // anchor, so their shading is fixed by the code, not by IP.
    std::vector<int> starts;
    bool synced = false;
    for (int a = base; a <= ip && a <= base + 16 && !synced; a++)
    {
        starts.clear();
        int cur = a;
        while (cur < ip) { starts.push_back(cur); cur += ArenaInsnLen(bytes, cur & 0xFFFF); }
        synced = (cur == ip);
    }
    // IP sits in code the anchor's stream never reaches (it jumped into the
    // middle of another framing): count from IP itself rather than show a
    // framing IP is not actually running.
    if (!synced) starts.clear();
    const int ipIndex = (int)starts.size();

    // behind IP: the anchor's own boundaries
    for (int j = 0; j < (int)starts.size(); j++)
    {
        const int s = starts[(size_t)j];
        if (s + cw::kOpBytes < lo) continue;
        if (j & 1) shade(s, ArenaInsnLen(bytes, s & 0xFFFF), lo, ip - 1);
    }
    // from IP on: carrying the same count forward
    int at = ip;
    for (int i = ipIndex; at <= hi; i++)
    {
        const int len = ArenaInsnLen(bytes, at & 0xFFFF);
        if (i & 1) shade(at, len, lo, hi);
        at += len;
    }
}

// The mouse during a wheel zoom. Where the view cannot hold still the byte the
// zoom started on, the mouse moves with that byte -- and for as long as the
// view glides, the mouse is clipped to that one pixel, so a hand on the mouse
// cannot fight it. The lock goes when the glide ends, or from AppFrame when
// the arena stops being drawn.
static int g_MouseLockFrame = -1;

static void ArenaMouseLock(ImVec2 p)
{
    // Once windows can be popped out, ImGui's coordinates are the desktop's
    // already, and the arena may be in a window other than the main one.
    // Otherwise they are the main window's, and its client area is on screen
    // where GLFW says.
    LONG x, y;
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        x = (LONG)lroundf(p.x);
        y = (LONG)lroundf(p.y);
    }
    else
    {
        const ImVec2 vp = ImGui::GetMainViewport()->Pos;
        int wx = 0, wy = 0;
        glfwGetWindowPos(g_Window, &wx, &wy);
        x = wx + (LONG)lroundf(p.x - vp.x);
        y = wy + (LONG)lroundf(p.y - vp.y);
    }
    RECT r = { x, y, x + 1, y + 1 };
    ClipCursor(&r);
    SetCursorPos(x, y);
    g_MouseLockFrame = g_FrameNo;
}

static void ArenaMouseUnlock()
{
    if (g_MouseLockFrame < 0) return;
    ClipCursor(NULL);
    g_MouseLockFrame = -1;
}

// Where the mouse belongs on one axis of a wheel zoom. `aScr` is where the
// wheel turned and `aByte` the byte that was under it -- outside 0..n when it
// was in the margin beside a smaller arena. `pan` and `cell` are the view now.
// - Over the arena: on that byte, wherever the view has put it.
// - In the margin: where it was, until the arena's near edge grows out to it,
//   and from then on riding that edge.
// The same numbers place it every frame, the last frame included, so it lands
// exactly on the byte instead of chasing it.
static float ArenaZoomMouseAxis(float aScr, float aByte, float pan, float cell, float n, float vmin, float vsz)
{
    float want;
    if (aByte < 0.0f)   want = ImMin(aScr, vmin + (0.0f - pan) * cell);
    else if (aByte > n) want = ImMax(aScr, vmin + (n - pan) * cell);
    else                want = vmin + (aByte - pan) * cell;
    return ImClamp(want, vmin, vmin + vsz - 1.0f);
}

// Centre the arena on an axis where the whole thing fits; otherwise keep the
// view inside it, so there is never dead space to pan into.
static void ArenaPlaceAxis(float& pan, float vis, float n)
{
    pan = (vis >= n) ? (n - vis) * 0.5f : ImClamp(pan, 0.0f, n - vis);
}

// Where the mouse will be when the zoom reaches `target`: the view it will
// settle on (the byte held at the point, then placed), and the spot on it.
// Worked out before the glide, so the mouse goes straight there -- chasing
// the byte frame by frame would swing out and back where the edge clamps and
// the centring take turns.
static float ArenaZoomMouseFinal(float aScr, float aByte, float target, float n, float vmin, float vsz)
{
    float pan = aByte - (aScr - vmin) / target;
    ArenaPlaceAxis(pan, vsz / target, n);
    return ArenaZoomMouseAxis(aScr, aByte, pan, target, n, vmin, vsz);
}

static void DrawArenaGrid(cw::GuiSession& g)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int N = 256;                                  // 256 x 256 bytes = the 0x10000 arena
    float cell = g_App.arenaZoom;

    ImGuiIO& io = ImGui::GetIO();

    // The view is driven by our own pan, in arena bytes, rather than by the
    // child's scroll. ImGui resolves a scroll target against the content size
    // measured at the end of the frame, so a wheel zoom could not read back the
    // position it had just asked for and repeated notches drifted off the
    // cursor. Panning here is immediate and exact.
    // The offset strips are also where pointer tags go, so they come back
    // whenever a pointer pair is being shown.
    const bool  showRulers = g_App.arenaRulers || g_App.hlMask != 0;
    const float lw = showRulers ? ImMax(ArenaRulerW(), ArenaTagW()) : 0.0f;
    const float lh = showRulers ? ArenaRulerH() : 0.0f;

    const ImVec2 wpos = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();
    const ImVec2 viewMin(wpos.x + lw, wpos.y + lh);           // where the grid starts
    const ImVec2 viewSz(ImMax(1.0f, wsz.x - lw), ImMax(1.0f, wsz.y - lh));

    ImGui::SetCursorScreenPos(viewMin);
    ImGui::InvisibleButton("##arenapan", viewSz,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                           ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool panning = ImGui::IsItemActive() && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);

    ImVec2& pan = g_App.arenaPan;                             // top-left byte on screen

    // The wheel sets a target zoom and the view glides to it over a few
    // frames, holding still the byte that was under the mouse wherever it
    // can. Where the arena's edge or its centring will not let it, the mouse
    // goes with the byte instead (ArenaZoomMouseAxis), locked there for the
    // glide. Anything else that sets the zoom (Find IP, the settings) is taken
    // as it stands.
    static float  zoomTarget = -1.0f, zoomSeen = -1.0f;
    static ImVec2 anchorScreen, anchorByte, mouseAt;
    static float  cellFrom = 1.0f;                  // the zoom when the notch came
    static bool   gesture = false;                  // a wheel zoom is gliding and owns the mouse
    if (zoomTarget < 0.0f || g_App.arenaZoom != zoomSeen) { zoomTarget = g_App.arenaZoom; gesture = false; }
    if (hovered && io.MouseWheel != 0.0f)
    {
        // a notch mid-glide carries on from where the glide has put the mouse
        const ImVec2 at = gesture ? mouseAt : io.MousePos;
        zoomTarget   = ImClamp(zoomTarget * powf(1.15f, io.MouseWheel), 1.0f, 512.0f);
        anchorScreen = at;
        anchorByte   = ImVec2(pan.x + (at.x - viewMin.x) / cell, pan.y + (at.y - viewMin.y) / cell);
        cellFrom     = cell;
        gesture      = (g_FollowIp < 0);            // following, the IP holds the middle instead
    }
    if (ImGui::IsItemActive())
    {
        pan.x -= io.MouseDelta.x / cell;
        pan.y -= io.MouseDelta.y / cell;
        anchorByte.x -= io.MouseDelta.x / cell;     // a drag mid-glide moves the anchor with it
        anchorByte.y -= io.MouseDelta.y / cell;
    }
    if (zoomTarget != cell)
    {
        const float k = 1.0f - expf(-io.DeltaTime * 16.0f);
        float nz = cell * powf(zoomTarget / cell, k);
        if (fabsf(logf(zoomTarget / nz)) < 0.002f) nz = zoomTarget;
        pan.x = anchorByte.x - (anchorScreen.x - viewMin.x) / nz;
        pan.y = anchorByte.y - (anchorScreen.y - viewMin.y) / nz;
        g_App.arenaZoom = cell = nz;
    }
    zoomSeen = g_App.arenaZoom;

    const float visX = viewSz.x / cell, visY = viewSz.y / cell;

    // Follow IP: the followed warrior's next instruction glides to the middle
    // of the view as the war plays. Dragging the arena yourself lets go.
    if (g_FollowIp >= 0)
    {
        if (panning) { g_FollowIp = -1; g_App.status = "Stopped following IP"; }
        else if (g.ready() && g_FollowIp < g.numWarriors())
        {
            const int off = g.warriorIpOffset(g_FollowIp);
            if (off >= 0)
            {
                const float tx = (float)(off % N) + 0.5f - visX * 0.5f;
                const float ty = (float)(off / N) + 0.5f - visY * 0.5f;
                if (fabsf(tx - pan.x) > 48.0f || fabsf(ty - pan.y) > 48.0f) pan = ImVec2(tx, ty);
                else
                {
                    const float k = 1.0f - expf(-io.DeltaTime * 14.0f);
                    pan.x += (tx - pan.x) * k;
                    pan.y += (ty - pan.y) * k;
                }
            }
        }
    }

    ArenaPlaceAxis(pan.x, visX, (float)N);
    ArenaPlaceAxis(pan.y, visY, (float)N);

    // The mouse goes straight from where the wheel turned to where the zoom's
    // byte will end up, as far along as the zoom is, locked there until the
    // glide ends -- the last frame puts it on the final spot exactly.
    if (gesture && g_FollowIp >= 0) { gesture = false; ArenaMouseUnlock(); }
    if (gesture)
    {
        const ImVec2 to(ArenaZoomMouseFinal(anchorScreen.x, anchorByte.x, zoomTarget, (float)N, viewMin.x, viewSz.x),
                        ArenaZoomMouseFinal(anchorScreen.y, anchorByte.y, zoomTarget, (float)N, viewMin.y, viewSz.y));
        const float t = (cell == zoomTarget || zoomTarget == cellFrom) ? 1.0f
                      : ImClamp(logf(cell / cellFrom) / logf(zoomTarget / cellFrom), 0.0f, 1.0f);
        mouseAt = ImVec2(anchorScreen.x + (to.x - anchorScreen.x) * t, anchorScreen.y + (to.y - anchorScreen.y) * t);
        ArenaMouseLock(mouseAt);
        if (cell == zoomTarget) { gesture = false; ArenaMouseUnlock(); }
    }

    const ImVec2 p0(viewMin.x - pan.x * cell, viewMin.y - pan.y * cell);   // cell 0,0

    const int x0 = ImMax(0, (int)floorf(pan.x)),        x1 = ImMin(N, (int)ceilf(pan.x + visX) + 1);
    const int y0 = ImMax(0, (int)floorf(pan.y)),        y1 = ImMin(N, (int)ceilf(pan.y + visY) + 1);
    dl->PushClipRect(viewMin, ImVec2(viewMin.x + viewSz.x, viewMin.y + viewSz.y), true);

    const cw::u8* bytes = g.arena();

    // The two hex digits are drawn at a size derived from the cell, so they
    // appear as soon as they can possibly fit and stay centred as you zoom --
    // fading in over the first few sizes rather than switching on at one.
    const float hexSize = ImFloor(cell * 0.56f);
    const bool  showHex = bytes && g_App.fontMono && hexSize >= 6.0f;
    const float hexFade = ImClamp((cell - 10.7f) / 6.0f, 0.0f, 1.0f);

    // Line width and corner rounding are straight proportions of the zoom and
    // of the thickness setting -- 0 is no lines and square corners, one solid
    // sheet -- with a pixel as the thinnest line. Lines exist only past
    // kGridOn, and fade in from transparent there (see ArenaLineScale).
    const float gridW    = ArenaLineScale(cell);
    const float gridFade = ArenaLineFade(cell);
    const float gap      = (gridW > 0.0f) ? ImClamp(cell * 0.07f * gridW, 1.0f, cell * 0.35f) : 0.0f;
    const float rounding = (gridW > 0.0f) ? ImMin(cell * 0.10f * gridW, cell * 0.30f) : 0.0f;
    const float half     = gap * 0.5f;     // each cell gives up half a line on every side
    if (showHex) ImGui::PushFont(g_App.fontMono, hexSize);
    const float digitW = showHex ? ImGui::CalcTextSize("FF").x : 0.0f;
    // Centred on the digits' own ink, not the font's line box -- the line box
    // has room for descenders, which left the digits sitting high in the cell.
    float digitMid = showHex ? ImGui::GetFontSize() * 0.5f : 0.0f;
    if (showHex)
        if (ImFontBaked* bk = ImGui::GetFont()->GetFontBaked(hexSize))
            if (const ImFontGlyph* gl = bk->FindGlyph((ImWchar)'0'))
                digitMid = (gl->Y0 + gl->Y1) * 0.5f * (hexSize / bk->Size);

    // The lines between the cells are not drawn as lines: each cell is inset by
    // half a line on every side and whatever is behind shows through. That puts
    // every line exactly on a cell boundary -- where the offsets in the strips
    // point -- and leaves each byte's digits centred in its square. Their
    // colour, the theme's, is one sheet laid under the visible cells.
    //
    // While the lines are fading in, each cell is first laid down whole and
    // square, the sheet goes over everything at the fade's opacity, and the
    // inset cell goes on top. So the gaps and the rounded corners show the
    // cell's own colour tinted towards the line colour -- the lines appear by
    // turning opaque, not by growing out of nothing.
    const bool fading = gap > 0.0f && gridFade < 1.0f;
    const ImVec2 sheetMin(p0.x + x0 * cell - half, p0.y + y0 * cell - half);
    const ImVec2 sheetMax(p0.x + x1 * cell + half, p0.y + y1 * cell + half);
    if (gap > 0.0f && !fading)
        dl->AddRectFilled(sheetMin, sheetMax, HexU(Col::ArenaGrid));

    // Instruction bands, worked out for this frame's state of the war.
    static std::vector<unsigned char> band(0x10000, 0);
    static std::vector<int> banded;
    for (int o : banded) band[(size_t)o] = 0;
    banded.clear();
    if (g_App.arenaInsnBands && bytes)
    {
        int only = -1;
        if (g_App.arenaInsnWho == 0)
        {
            only = g_App.inspectWarrior;
            if (only < 0 || only >= g.numWarriors()) only = g.player().actingWarrior();
        }
        for (int wi = 0; wi < g.numWarriors(); wi++)
        {
            if (only >= 0 && wi != only) continue;
            if (only < 0 && !g.warriorAlive(wi)) continue;
            const int ip = g.warriorIpOffset(wi);
            if (ip >= 0) BandInstructions(g, bytes, ip, g_App.arenaInsnReach, band, banded);
        }
    }

    // Untouched core, in the theme's arena colour.
    const ImU32 coreCol = HexU(Col::ArenaCore);
    const ImU32 bandInk = HexU(0x000000);
    const ImU32 coreBandInk = HexU(Col::Text);
    // Digits are dark on a light cell and light on a dark one, whatever the
    // theme or the warrior colour; on untouched core they are quieter.
    auto inkFor = [&](ImU32 cellCol, bool owned) -> ImU32 {
        const ImVec4 c = ImGui::ColorConvertU32ToFloat4(cellCol);
        const float lum = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
        const ImU32 ink = HexU(lum > 0.55f ? 0x000000 : 0xFFFFFF);
        ImU32 t = MixOpaque(cellCol, ink, owned ? 0.80f : 0.45f);
        return (t & ~IM_COL32_A_MASK) | ((ImU32)(hexFade * 255.0f + 0.5f) << IM_COL32_A_SHIFT);
    };

    // Each owner's cell colour, plain and banded, worked out once a frame rather
    // than once a byte -- a war can own all 65536 of them.
    // Solid, always: a dead survivor's cells are its colour faded into the core
    // colour rather than made see-through.
    static std::vector<ImU32> ownCol, ownBand;
    const int nw = ImMax(0, g.numWarriors());
    ownCol.resize((size_t)nw + 1);
    ownBand.resize((size_t)nw + 1);
    ownCol[0]  = coreCol;                                   // [0] is untouched core
    ownBand[0] = MixOpaque(coreCol, coreBandInk, 0.14f);
    for (int wi = 0; wi < nw; wi++)
    {
        ownCol[(size_t)wi + 1]  = MixOpaque(coreCol, WarriorColor(wi, 1.0f), g.warriorAlive(wi) ? 1.0f : 0.36f);
        ownBand[(size_t)wi + 1] = MixOpaque(ownCol[(size_t)wi + 1], bandInk, 0.26f);
    }
    auto cellColour = [&](int off, int wi) -> ImU32 {
        const size_t k = (wi >= 0 && wi < nw) ? (size_t)wi + 1 : 0;
        return band[(size_t)off] ? ownBand[k] : ownCol[k];
    };

    if (fading)
    {
        // whole square cells, then the line sheet over them, see-through
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
            {
                const int off = y * N + x;
                const ImVec2 a(p0.x + x * cell, p0.y + y * cell);
                dl->AddRectFilled(a, ImVec2(a.x + cell, a.y + cell), cellColour(off, g.ownerAt(off)));
            }
        dl->AddRectFilled(sheetMin, sheetMax, HexU(Col::ArenaGrid, gridFade));
    }

    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
        {
            const int off = y * N + x;
            const int wi  = g.ownerAt(off);
            const ImU32 col = cellColour(off, wi);
            const ImVec2 a(p0.x + x * cell, p0.y + y * cell);
            dl->AddRectFilled(ImVec2(a.x + half, a.y + half), ImVec2(a.x + cell - half, a.y + cell - half),
                              col, rounding);
            if (showHex && hexFade > 0.0f)
            {
                char h[3];
                static const char* kHex = "0123456789ABCDEF";
                h[0] = kHex[(bytes[off] >> 4) & 0xF];
                h[1] = kHex[bytes[off] & 0xF];
                h[2] = 0;
                const ImU32 tc = inkFor(col, wi >= 0);
                dl->AddText(ImVec2(a.x + (cell - digitW) * 0.5f, a.y + cell * 0.5f - digitMid), tc, h);
            }
        }
    if (showHex) ImGui::PopFont();

    // which byte is under the cursor
    g_App.arenaHoverOff = -1;
    if (hovered)
    {
        const int hx = (int)((io.MousePos.x - p0.x) / cell);
        const int hy = (int)((io.MousePos.y - p0.y) / cell);
        if (hx >= 0 && hx < N && hy >= 0 && hy < N) g_App.arenaHoverOff = hy * N + hx;

        if (g_App.editMode && bytes && g_App.arenaHoverOff >= 0 &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            RequestByteEdit(cw::SNAP_LO + g_App.arenaHoverOff,
                            bytes[g_App.arenaHoverOff], "arena");
    }
    // a byte being edited: the field goes over its cell, wherever the view has
    // put that this frame
    if (EditIsByte(g_Edit.linear))
    {
        const int eo = (int)(g_Edit.linear - cw::SNAP_LO);
        const int ex = eo % N, ey = eo / N;
        if (eo >= 0 && eo < N * N && ex >= x0 && ex < x1 && ey >= y0 && ey < y1)
        {
            const ImVec2 a(p0.x + ex * cell, p0.y + ey * cell);
            EditPlace(ImVec2(a.x + half, a.y + half), ImVec2(a.x + cell - half, a.y + cell - half));
        }
    }

    // IP markers: a ring on the cell each live warrior is about to execute. The
    // ring follows the cells' own rounding so it does not read as a stray square
    // once the grid is zoomed in far enough to round its corners.
    const float ipRound = (rounding > 0.0f) ? rounding + 1.0f : 0.0f;
    for (int wi = 0; wi < g.numWarriors(); wi++)
    {
        const int off = g.warriorIpOffset(wi);
        if (off < 0) continue;
        const int x = off % N, y = off / N;
        if (x < x0 - 1 || x > x1 || y < y0 - 1 || y > y1) continue;
        const ImVec2 a(p0.x + x * cell, p0.y + y * cell);
        const ImVec2 mn(a.x + half - 1, a.y + half - 1), mx(a.x + cell - half + 1, a.y + cell - half + 1);
        // the warrior's colour, and a hairline between it and the cell: black,
        // or white when that colour is a dark one
        dl->AddRect(mn, mx, WarriorColor(wi, 1.0f), ipRound, 0, ImMax(1.5f, cell * 0.16f));
        dl->AddRect(mn, mx, HairlineFor(WarriorColor(wi, 1.0f), 0.9f), ipRound, 0, 1.0f);
    }

    ArenaPointerRings(g, p0, cell, N, x0, y0, x1, y1);
    dl->PopClipRect();
    if (showRulers)
    {
        ArenaRulers(p0, cell, N, lw, lh);
        ArenaPointerTags(p0, cell, N, lw, lh);
    }
    // the edit field, at the size the digits are drawn at (within reason)
    EditField(ImClamp(hexSize, g_App.fontMonoSize * g_Scale, 40.0f * g_Scale));
}

// A text toggle; several in a row with no gap form a train (TrainCorners).
static bool ToggleChip(const char* id, const char* label, bool* v, const char* tip,
                       int trainIndex = 0, int trainCount = 1)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float h = kBarH * g_Scale;
    const float w = ts.x + 18 * g_Scale;
    BtnState b = BareButton(id, ImVec2(w, h));
    const bool on = *v;
    dl->AddRectFilled(b.mn, b.mx,
                      HexU(on ? Col::Accent : (b.held ? Col::Bg4 : b.hovered ? Col::Bg3 : Col::Bg2),
                           on ? (b.hovered ? 0.95f : 0.85f) : 1.0f),
                      7.0f * g_Scale, TrainCorners(trainIndex, trainCount));
    // a lit carriage needs no joint; two dark ones side by side do
    if (!on)
    {
        bool prevOn = false;
        ImGuiStorage* st = ImGui::GetStateStorage();
        prevOn = st->GetBool(ImGui::GetID("##trainprev"), false);
        if (!prevOn) TrainJoint(dl, b.mn, b.mx, trainIndex);
    }
    ImGui::GetStateStorage()->SetBool(ImGui::GetID("##trainprev"), on);
    dl->AddText(ImVec2((b.mn.x + b.mx.x - ts.x) * 0.5f, (b.mn.y + b.mx.y - ts.y) * 0.5f),
                HexU(on ? Col::Bg0 : (b.hovered ? Col::Text : Col::TextDim)), label);
    if (b.hovered && tip) ImGui::SetTooltip("%s", tip);
    if (b.clicked) *v = !*v;
    return b.clicked;
}

// The playback speed as a slim bar with its value written inside it: drag
// along it, or scroll over it. Full is the fastest.
static void SpeedBar(float w)
{
    AppState& S = g_App;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = kBarH * g_Scale;
    BtnState b = BareButton("##speedbar", ImVec2(w, h));
    const ImGuiIO& io = ImGui::GetIO();
    if (b.held)
        S.speedExp = ImClamp((io.MousePos.x - b.mn.x) / w, 0.0f, 1.0f) * 30.0f;
    if (b.hovered && io.MouseWheel != 0.0f)
        S.speedExp = ImClamp(S.speedExp + io.MouseWheel, 0.0f, 30.0f);
    if (b.hovered || b.held) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    const float r = 7.0f * g_Scale;
    const float t = ImClamp(S.speedExp / 30.0f, 0.0f, 1.0f);
    dl->AddRectFilled(b.mn, b.mx, HexU(b.hovered || b.held ? Col::Bg3 : Col::Bg2), r);
    if (t > 0.0f)
    {
        // the fill keeps the bar's left corners and is cut square at its end,
        // until it reaches the right edge
        const float fx = b.mn.x + w * t;
        dl->AddRectFilled(b.mn, ImVec2(fx, b.mx.y), HexU(Col::Accent2, b.held ? 0.42f : 0.30f), r,
                          t >= 0.999f ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
    }
    char txt[32];
    const double rps = RoundsPerSec();
    if (rps >= 10000.0) snprintf(txt, sizeof(txt), "%.0fk r/s", rps / 1000.0);
    else                snprintf(txt, sizeof(txt), "%.0f r/s", rps);
    const ImVec2 ts = ImGui::CalcTextSize(txt);
    dl->AddText(ImVec2((b.mn.x + b.mx.x - ts.x) * 0.5f, (b.mn.y + b.mx.y - ts.y) * 0.5f),
                HexU(Col::Text), txt);
    if (b.hovered && !b.held)
        ImGui::SetTooltip("Playback speed, in rounds per second\nDrag along the bar, or scroll over it");
}

#include "gui/gui_timeline.inl"   // step-column / survivor-lane timeline

// A grab strip between the arena and the inspector. `sign` is +1 when the
// inspector is to the left of it (dragging right widens it) and -1 when it is
// to the right.
static void InspectorSplitter(const char* id, float sign, float h)
{
    const float w = 5.0f * g_Scale;
    ImGui::InvisibleButton(id, ImVec2(w, h));
    const bool act = ImGui::IsItemActive();
    if (act) g_App.inspectorWidth = ImClamp(
        g_App.inspectorWidth + sign * ImGui::GetIO().MouseDelta.x / g_Scale, 240.0f, 1200.0f);
    if (act || ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
        HexU(act ? Col::Accent : Col::Line, (act || ImGui::IsItemHovered()) ? 1.0f : 0.5f));
}

// Centre the arena on the instruction the inspected survivor is about to run.
// A warrior whose CS has been pushed out of the arena has no cell to point at.
static void FocusArenaOnIp(int wi)
{
    AppState& S = g_App;
    if (!S.game.ready()) return;
    if (wi < 0) wi = S.inspectWarrior;
    if (wi < 0 || wi >= S.game.numWarriors()) wi = S.game.player().actingWarrior();
    if (wi < 0 || wi >= S.game.numWarriors()) { S.status = "No survivor selected"; return; }

    const int off = S.game.warriorIpOffset(wi);
    if (off < 0)
    {
        const cw::CpuState& st = S.game.player().cpu(wi);
        char b[96];
        snprintf(b, sizeof(b), "%s is executing outside the arena (CS %04X)",
                 S.game.replay().warriorName[(size_t)wi].c_str(), (unsigned)(cw::u16)st.cs);
        S.status = b;
        return;
    }
    S.arenaZoom = ImMax(S.arenaZoom, 16.0f);
    S.arenaPan = ImVec2((float)(off % 256) - 8.0f, (float)(off / 256) - 6.0f);
    S.status = "Centred on " + S.game.replay().warriorName[(size_t)wi];
}

static void DrawArenaPanel()   // window title: "Debug"
{
    AppState& S = g_App;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    // The transport bar is wider than the panel on a narrow layout; letting the
    // window scroll to reach it dragged the whole arena off screen instead.
    const bool open = ImGui::Begin("Debug", NULL, ImGuiWindowFlags_NoCollapse |
                                                 ImGuiWindowFlags_NoScrollbar |
                                                 ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (open)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float barH = 34.0f * g_Scale;
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float  w = ImGui::GetContentRegionAvail().x;
        dl->AddRectFilled(o, ImVec2(o.x + w, o.y + barH), HexU(Col::Bg1));
        dl->AddLine(ImVec2(o.x, o.y + barH - 1), ImVec2(o.x + w, o.y + barH - 1), HexU(Col::Line));

        const bool busy  = S.game.precomputing();
        const bool ready = S.game.ready();
        const bool run   = S.game.running();

        ImGui::SetCursorScreenPos(ImVec2(o.x + 10 * g_Scale, o.y + (barH - 28 * g_Scale) * 0.5f));

        // transport, one train: back | play/pause | forward | to the next
        // event | rewind. Everything moves in whole rounds -- one pass in
        // which every living warrior takes its turn. Play wears the accent.
        const float group = 10.0f * g_Scale;   // the gap between two trains
        if (ToolButton("##back", 7, "Back - one round (Shift+F11)", false, ready,
                       TrainCorners(0, 5), 0)) GameStepRound(-1);
        ImGui::SameLine(0, 0);
        if (ToolButton("##dbgrun", run ? 1 : 0,
                       busy ? "Precomputing..." : (run ? "Pause (F5)" : (ready ? "Run (F5)" : "Precompute + play (F5)")),
                       true, !busy, TrainCorners(1, 5), 1))
            GamePlayPause();
        ImGui::SameLine(0, 0);
        if (ToolButton("##fwd", 8, "Forward - one round (F11)", false, ready, TrainCorners(2, 5), 0))
            GameStepRound(+1);
        ImGui::SameLine(0, 0);
        if (ToolButton("##cont", 9, "Continue to the next event - death or BRK (F8)", false, ready,
                       TrainCorners(3, 5), 3)) GameContinue();
        ImGui::SameLine(0, 0);
        if (ToolButton("##rew", 3, "Rewind to the start (Shift+F5)", false, ready, TrainCorners(4, 5), 4))
            GameReset();

        // the speed, its value written inside the bar
        ImGui::SameLine(0, group);
        SpeedBar(112 * g_Scale);

        // seed + recompute, one train: the field and the button that uses it
        ImGui::SameLine(0, group);
        {
            const float h = kBarH * g_Scale, r = 7.0f * g_Scale;
            const float fieldW = 92 * g_Scale;
            const ImVec2 fmn = ImGui::GetCursorScreenPos();
            const bool hot = ImGui::IsMouseHoveringRect(fmn, ImVec2(fmn.x + fieldW, fmn.y + h));
            dl->AddRectFilled(fmn, ImVec2(fmn.x + fieldW, fmn.y + h), HexU(hot ? Col::Bg3 : Col::Bg2), r,
                              ImDrawFlags_RoundCornersLeft);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                ImVec2(9 * g_Scale, (h - ImGui::GetFontSize()) * 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
            ImGui::SetNextItemWidth(fieldW);
            if (ImGui::InputTextWithHint("##seed", "seed", S.seedText, sizeof(S.seedText),
                                         ImGuiInputTextFlags_EnterReturnsTrue))
                GameArm();
            if (ImGui::IsItemActive())
                dl->AddRect(fmn, ImVec2(fmn.x + fieldW, fmn.y + h), HexU(Col::Accent, 0.7f), r,
                            ImDrawFlags_RoundCornersLeft, 1.2f);
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Seed - text or a number, resolved the way the reference does:\n"
                                  "a plain number is used as-is, anything else is hashed.\n"
                                  "-> %lld   (Enter plays the war again)", (long long)SeedFromText(S.seedText));
            ImGui::SameLine(0, 0);
            if (ToolButton("##recompute", 10,
                           busy ? "Precomputing..." : "Recompute - play the whole war out again from this seed (Ctrl+F5)",
                           false, !busy, ImDrawFlags_RoundCornersRight, 1))
                GameArm();
        }

        // the three modes, one train: no gaps, rounded only at its two ends
        ImGui::SameLine(0, group);
        ToggleChip("##dbgstop", "DBG", &S.game.stopOnEvents, "Stop when a death or breakpoint is reached", 0, 3);
        ImGui::SameLine(0, 0);
        ToggleChip("##dbgrev", "REV", &S.game.reverse, "Run and continue backwards through the recording", 1, 3);
        ImGui::SameLine(0, 0);
        ToggleChip("##dbgedit", "EDIT", &S.editMode,
                   "Double-click a byte, a register or a flag to change it.\n"
                   "The war is played out again with the change on the next move.", 2, 3);

        ImGui::SameLine(0, 14 * g_Scale);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (kBarH * g_Scale - ImGui::GetTextLineHeight()) * 0.5f);
        if (busy)
            ImGui::TextColored(Hex(Col::Warn), "precomputing  %d rounds  (%.0f%%)",
                               S.game.progressRounds(), S.game.progress() * 100.0f);
        else
            ImGui::TextColored(Hex(Col::TextDim), "%s", S.game.status().c_str());
        if (S.arenaHoverOff >= 0 && S.game.arena())
        {
            ImGui::SameLine(0, 14 * g_Scale);
            const int wi = S.game.ownerAt(S.arenaHoverOff);
            ImGui::TextColored(Hex(Col::Accent2), "byte %04X = %02X",
                               (unsigned)S.arenaHoverOff,
                               (unsigned)S.game.arena()[S.arenaHoverOff]);
            if (wi >= 0)
            {
                ImGui::SameLine(0, 6 * g_Scale);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(WarriorColor(wi, 1.0f)),
                                   "%s", S.game.replay().warriorName[(size_t)wi].c_str());
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + barH));

        // Arena fills the space between the transport bar and the timeline,
        // which takes the bottom strip of whatever room the panel has.
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float tlMin = 96.0f * g_Scale;
        const float tlH   = S.showTimeline
                          ? ImClamp(g_TimelineWantH, tlMin, ImMax(tlMin, avail.y * 0.62f))
                          : 0.0f;
        // the child and the timeline are separate items, so their spacing counts
        // against the height too -- without this the panel overflows by a few
        // pixels and grows a scrollbar
        const float gridH = ImMax(60.0f, avail.y - tlH - ImGui::GetStyle().ItemSpacing.y);

        // A survivor that has been popped out into its own window is not also
        // shown in the pane; the pane comes back when that window is closed.
        bool insPopped = false;
        {
            int shown = S.inspectWarrior;
            if (shown < 0 && S.game.ready()) shown = S.game.player().actingWarrior();
            for (int x : S.poppedInspectors) if (x == shown) insPopped = true;
        }
        const float insW = (S.showInspector && !insPopped && avail.x > 520.0f * g_Scale)
                         ? ImClamp(S.inspectorWidth * g_Scale, 240.0f * g_Scale, avail.x * 0.62f)
                         : 0.0f;
        const float arenaW = ImMax(120.0f, avail.x - insW);

        // the inspector can take either side; the arena keeps the rest
        if (insW > 0.0f && S.inspectorLeft)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * g_Scale, 6 * g_Scale));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg1));
            ImGui::BeginChild("##inspector", ImVec2(insW, gridH), ImGuiChildFlags_Borders |
                                                                  ImGuiChildFlags_AlwaysUseWindowPadding);
            DrawWarriorInspector(S.game, S.inspectWarrior, false);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SameLine(0, 0);
            InspectorSplitter("##inssplitL", +1.0f, gridH);
            ImGui::SameLine(0, 0);
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg0));
        // No scrollbars: the wheel zooms, dragging pans, and the edge rulers say
        // where you are, so the bars were only eating pixels.
        ImGui::BeginChild("##arenaview", ImVec2(arenaW, gridH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoScrollWithMouse);
        if (S.game.arena())
        {
            DrawArenaGrid(S.game);   // picks its own hex font size from the zoom
        }
        else
        {
            ImGui::Dummy(ImVec2(0, 12 * g_Scale));
            ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
            ImGui::TextWrapped(busy
                ? "  Precomputing the war..."
                : "  No war recorded yet. Open a folder of survivors, enable at least two "
                  "groups, then press Play - the whole fight is played out silently first, "
                  "then replayed here.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (insW > 0.0f && !S.inspectorLeft)
        {
            ImGui::SameLine(0, 0);
            InspectorSplitter("##inssplitR", -1.0f, gridH);
            ImGui::SameLine(0, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * g_Scale, 6 * g_Scale));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg1));
            ImGui::BeginChild("##inspector", ImVec2(insW, gridH), ImGuiChildFlags_Borders |
                                                                  ImGuiChildFlags_AlwaysUseWindowPadding);
            DrawWarriorInspector(S.game, S.inspectWarrior, false);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        if (S.showTimeline) DrawTimeline(S.game, tlH);
    }
    ImGui::End();
}

// The competition and network clock. Not ImGui::GetTime(): that only moves when
// a frame is drawn, and the competition has to keep polling -- sockets, lanes,
// retry timers -- while the window is minimized and no frames are drawn.
static double AppNow() { return glfwGetTime(); }

#include "gui/gui_gpulane.inl"   // the Vulkan engine, run as a child process
#include "gui/gui_javalane.inl"  // the CGX Java engine, run the same way
#include "gui/gui_net.inl"       // the host / join wire
// the smart check's state and button live in gui_smartcheck.inl, after the
// competition code it builds on; the toolbar needs these two first
static bool AnyGroupUsesInt89();
static void SmartCheckToggle();
static bool SmartCheckIsOpen();
static bool SmartCheckIsRunning();
#include "gui/gui_compete.inl"   // tournament runner + score board
#include "gui/gui_smartcheck.inl" // which AX:DX an int 0x89 should look for

static void DrawStatusBar(float height)
{
    AppState& S = g_App;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;

    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + height), HexU(Col::Bg0));
    dl->AddLine(o, ImVec2(o.x + w, o.y), HexU(Col::Line));

    const float ty = o.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddCircleFilled(ImVec2(o.x + 14 * g_Scale, o.y + height * 0.5f), 3 * g_Scale,
                        HexU(S.game.running() ? Col::Ok : Col::TextDim));
    dl->AddText(ImVec2(o.x + 24 * g_Scale, ty), HexU(Col::TextDim), S.status.c_str());

    char right[192];
    if (S.open.Valid())
    {
        const TextEditor::Coordinates cp = S.editor.GetCursorPosition();
        snprintf(right, sizeof(right), "%d group(s)  |  %d zombie(s)  |  %d selected  |  Ln %d, Col %d  |  %d lines",
                 (int)S.players.size(), (int)S.zombies.size(), (int)S.selection.size(),
                 cp.mLine + 1, cp.mColumn + 1, S.editor.GetTotalLines());
    }
    else
    {
        snprintf(right, sizeof(right), "%d group(s)  |  %d zombie(s)  |  %d selected",
                 (int)S.players.size(), (int)S.zombies.size(), (int)S.selection.size());
    }
    const ImVec2 rsz = ImGui::CalcTextSize(right);
    dl->AddText(ImVec2(o.x + w - rsz.x - 14 * g_Scale, ty), HexU(Col::TextDim), right);

    ImGui::Dummy(ImVec2(w, height));
}

static void DrawAboutPopup()
{
    if (g_App.showAbout && !ImGui::IsPopupOpen("About Core Wars 8086"))
        ImGui::OpenPopup("About Core Wars 8086");

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About Core Wars 8086", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextColored(Hex(Col::Accent), "CORE WARS 8086");
        ImGui::TextColored(Hex(Col::TextDim), "debugger / IDE shell");
        ImGui::SeparatorText("Workspace");
        ImGui::BulletText("Click a player or a survivor button to select and open it.");
        ImGui::BulletText("Ctrl+click adds to the selection, Shift+click extends it.");
        ImGui::BulletText("Hold (long click) a button or a player row to disable it.");
        ImGui::BulletText("Ctrl+Q disables everything selected - press again to re-enable.");
        ImGui::BulletText("Right-click any row for per-item actions.");
        ImGui::SeparatorText("Files");
        ImGui::BulletText("survivor1.asm + survivor2.asm load as one player.");
        ImGui::BulletText("Names that differ only by a trailing number are paired.");
        ImGui::SeparatorText("Keys");
        ImGui::BulletText("Ctrl+N new player    Ctrl+O load    Ctrl+S save    Ctrl+Shift+S save all");
        ImGui::BulletText("F5 run / pause    F11 step a round    Shift+F5 reset    Esc clear selection");
        ImGui::Dummy(ImVec2(0, 8 * g_Scale));
        if (ImGui::Button("Close", ImVec2(130 * g_Scale, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            g_App.showAbout = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    else
    {
        g_App.showAbout = false;
    }
}

// -----------------------------------------------------------------------------
// Keyboard shortcuts
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Settings file
//
// A flat key=value list beside the executable. Only preferences live here --
// never the workspace, which comes from whatever folder was opened.
// -----------------------------------------------------------------------------
static std::string SettingsPath()
{
    return ExeDir() + "cwest.ini";
}

// One list, walked twice: once to write and once to read, so a new setting
// cannot be saved but not loaded.
#define CWEST_SETTINGS(B, I, F, U, S)                                          \
    B(inspectorLeft)      B(groupsCollapsed)   B(showInspector)                \
    B(popOutWindows)                                                           \
    B(showTimeline)      B(showEditor)        B(showArena)      B(showBoard)  \
    B(arenaRulers)        B(advanceSeed)                                       \
    B(compBarLabels)      B(compBarShadow)                                     \
    B(compSeedInBar)      B(compAllowBusy)     B(compSeekTool)                 \
    B(autoSave)                                                                \
    B(asmUpperCase)       B(showAsmGutter)     B(dblClickDisasm)               \
    B(followSS)           B(followES)          B(followCS)                     \
    B(arenaInsnBands)     I(arenaInsnWho)      I(arenaInsnReach)               \
    U(hlMask)                                                                  \
    B(breakStartsWith)    B(breakOnTimeline)                                   \
    B(folderAssemble)     B(folderDisassemble)                                 \
    I(themeIndex)         I(folderSortMode)    I(zombieSpeed)   I(maxRounds)   \
    B(compBarsUp)         B(compBarsFit)                                       \
    I(compGames)          I(compThreads)       I(compByGroup)                  \
    I(compFieldShow)                                                           \
    I(compCombo)          I(compPairSeed)      I(compBackend)                  \
    I(compMode)           I(compNetPort)                                       \
    I(compNetProtect)     S(compNetPassword)                                   \
    I(exportFormat)       B(exportKeepFolders) S(exportDir)                    \
    F(inspectorWidth)     F(workspaceWidth)    F(fontUISize)                   \
    F(fontMonoSize)       F(fontEditorSize)    F(speedExp)      F(arenaZoom)   \
    F(arenaGridWidth)                                                          \
    U(hlColor)                                                                 \
    U(groupPal0)          U(groupPal1)         U(groupPal2)                    \
    U(groupPal3)          U(groupPal4)         I(groupPalNext)                 \
    S(seedText)           S(breakPatterns)     S(compNetHost)   S(compNetName) \
    S(fontUIFile)         S(fontMonoFile)      S(compSeedText)

static void SaveSettings()
{
    std::ofstream f(SettingsPath().c_str(), std::ios::trunc);
    if (!f) return;
    const AppState& S = g_App;
#define WB(n) f << #n << "=" << (S.n ? 1 : 0) << "\n";
#define WI(n) f << #n << "=" << S.n << "\n";
#define WF(n) f << #n << "=" << S.n << "\n";
#define WU(n) f << #n << "=" << S.n << "\n";
#define WS(n) f << #n << "=" << S.n << "\n";
    CWEST_SETTINGS(WB, WI, WF, WU, WS)
#undef WB
#undef WI
#undef WF
#undef WU
#undef WS
    f << "lastFolder=" << S.lastFolder << "\n";
    for (size_t i = 0; i < S.recentWorkspaces.size() && i < 8; i++)
        f << "recent" << i << "=" << S.recentWorkspaces[i] << "\n";
    // the palette, so a hand-edited theme survives
    for (int i = 0; i < kPalCount; i++) f << "pal" << i << "=" << *PalSlot(i) << "\n";
}

static void LoadSettings()
{
    std::ifstream f(SettingsPath().c_str());
    if (!f) return;
    AppState& S = g_App;
    std::string line;
    bool havePal[kPalCount] = {};
    // the arena's own colour settings from before they joined the palette
    bool legBgCustom = false, legGridCustom = false;
    unsigned legBg = 0, legGrid = 0x4C8DFF;
    while (std::getline(f, line))
    {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);
        if (k == "arenaBgCustom")   { legBgCustom   = atoi(v.c_str()) != 0; continue; }
        if (k == "arenaGridCustom") { legGridCustom = atoi(v.c_str()) != 0; continue; }
        if (k == "arenaBg")         { legBg   = (unsigned)strtoul(v.c_str(), NULL, 10); continue; }
        if (k == "arenaGrid")       { legGrid = (unsigned)strtoul(v.c_str(), NULL, 10); continue; }
#define RB(n) if (k == #n) { S.n = (atoi(v.c_str()) != 0); continue; }
#define RI(n) if (k == #n) { S.n = atoi(v.c_str()); continue; }
#define RF(n) if (k == #n) { S.n = (float)atof(v.c_str()); continue; }
#define RU(n) if (k == #n) { S.n = (unsigned)strtoul(v.c_str(), NULL, 10); continue; }
#define RS(n) if (k == #n) { snprintf(S.n, sizeof(S.n), "%s", v.c_str()); continue; }
        CWEST_SETTINGS(RB, RI, RF, RU, RS)
#undef RB
#undef RI
#undef RF
#undef RU
#undef RS
        // opened once the app is up, not here: until then there is no workspace
        if (k == "lastFolder") { g_StartWorkspace = v; continue; }
        if (k.compare(0, 6, "recent") == 0) { if (!v.empty()) S.recentWorkspaces.push_back(v); continue; }
        if (k.compare(0, 3, "pal") == 0)
        {
            const int i = atoi(k.c_str() + 3);
            if (i >= 0 && i < kPalCount) { *PalSlot(i) = (unsigned)strtoul(v.c_str(), NULL, 10); havePal[i] = true; }
        }
    }

    // GPU alone is gone, and the busy lane only stays chosen while it is allowed.
    if (S.compBackend == COMP_GPU || S.compBackend < 0 || S.compBackend > COMP_JAVA) S.compBackend = COMP_CPU;
    if (S.compBackend == COMP_BOTH && !S.compAllowBusy) S.compBackend = COMP_CPU;
    if (S.folderSortMode != 1) S.folderSortMode = 0;

    // A file saved before the arena joined the palette has no pal13. In it,
    // "Custom" was index 4 -- which is a preset now.
    const bool oldFile = !havePal[13];
    if (oldFile && S.themeIndex == 4) S.themeIndex = kThemeCount;
    if (S.themeIndex < 0 || S.themeIndex > kThemeCount) S.themeIndex = 0;

    if (S.themeIndex < kThemeCount)
    {
        // A preset is always the preset as it is now; the saved colours only
        // matter for a hand-edited palette.
        for (int i = 0; i < kPalCount; i++) *PalSlot(i) = kThemes[S.themeIndex].c[i];
    }
    else
    {
        // a hand-edited palette from an older file: the slots it never had
        // come from whichever preset it is closer to, light or dark
        const float lum = 0.2126f * ((Col::Bg1 >> 16) & 0xFF) + 0.7152f * ((Col::Bg1 >> 8) & 0xFF)
                        + 0.0722f * (Col::Bg1 & 0xFF);
        const ThemeDef& closest = kThemes[lum < 128.0f ? 0 : 1];
        for (int i = 0; i < kPalCount; i++) if (!havePal[i]) *PalSlot(i) = closest.c[i];
    }

    // The old per-arena colour overrides are kept: laid over the theme, they
    // make it a Custom palette.
    if (oldFile)
    {
        bool changed = false;
        if (legBgCustom) { Col::ArenaCore = legBg; changed = true; }
        if (legGridCustom && legGrid != 0x4C8DFF) { Col::ArenaGrid = legGrid; changed = true; }
        if (changed) S.themeIndex = kThemeCount;
    }
}

// the font pickers in the options window use these (defined with LoadFonts)
static std::vector<std::string> FontDirs();
static std::vector<std::string> ListFontFiles();
static std::string FontDisplayName(const std::string& file);
#include "gui/gui_options.inl"   // options window, folder flow, zombie section

static void HandleShortcuts()
{
    AppState& S = g_App;
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Q))              ToggleSelectionEnabled();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))              QueueWsOp(WsOp::NewGroup, TargetFolder());
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))              QueueWsOp(WsOp::PickWorkspace);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Comma))          g_App.showOptions = !g_App.showOptions;

    // The list keys -- never while a text field or the source editor has the
    // keyboard, and (but for Delete and Paste) only when the groups bar had the
    // last click.
    const bool listKeys = !io.WantTextInput && HasWorkspace();
    if (listKeys && !g_App.selection.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        DeleteSelection();
    if (listKeys && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V))
        QueueWsOp(WsOp::Paste, TargetFolder());
    if (listKeys && g_GroupsFocus)
    {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_A)) SelectAllRows();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C)) CopySelectionToClipboard();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z) && !g_Trash.empty()) QueueWsOp(WsOp::Undo);
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && !g_App.selection.empty())
        {
            Ref r = g_App.anchor.Valid() && IsSelected(g_App.anchor) ? g_App.anchor : g_App.selection.back();
            if (!IsFolderRef(r) && !IsZombieRef(r)) r.slot = -1;     // a survivor renames its group
            BeginInlineRename(r);
        }
    }

#ifdef CWEST_TEST_HOOKS
    // Test builds only: F9 runs the selected groups against every top-level
    // folder, the same op the group menu queues.
    if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
    {
        WsOp op;
        op.kind = WsOp::RunFolders;
        for (const Ref& r : S.selection) { RefKey k = KeyOfRef(r); if (k.kind == 0) op.items.push_back(k); }
        for (int fi : FolderChildren(std::string())) op.paths.push_back(S.folders[fi].path);
        QueueWsOp(op);
    }
    // F8 opens the run-against-folders picker for the selected groups
    if (ImGui::IsKeyPressed(ImGuiKey_F8, false))
    {
        g_RunPicker.groups.clear();
        for (const Ref& r : S.selection) { RefKey k = KeyOfRef(r); if (k.kind == 0) g_RunPicker.groups.push_back(k); }
        if (!g_RunPicker.groups.empty()) { g_RunPicker.open = true; g_RunPicker.appearing = true; }
    }
    // and Ctrl+F12 crashes on purpose, to check what crash.log says
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F12))
    {
        volatile int* nowhere = nullptr;
        *nowhere = 1;
    }
#endif

    // Ctrl +/- sizes the source editor, the way every editor does it
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Equal) ||
        ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd))
        g_App.fontEditorSize = ImMin(40.0f, g_App.fontEditorSize + 1.0f);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Minus) ||
        ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract))
        g_App.fontEditorSize = ImMax(8.0f, g_App.fontEditorSize - 1.0f);

    // Re-reading the code from a byte replaces the whole buffer, which the text
    // editor cannot undo on its own, so it gets its own one-level-per-step
    // history. The editor keeps first claim on the shortcut while it has
    // ordinary edits of its own to undo.
    {
        Survivor* sv = SurvivorAt(g_App.open);
        if (sv && sv->present && !(listKeys && g_GroupsFocus))
        {
            if (!g_App.editor.CanUndo() && !g_ReframeUndo.empty() &&
                ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
            {
                g_ReframeRedo.push_back(sv->code);
                sv->code = g_ReframeUndo.back();
                g_ReframeUndo.pop_back();
                ReassembleSurvivor(*sv);
                g_App.editor.SetText(sv->code);
                PushEditorMarkers(*sv);
                g_App.status = "Undid the re-read";
            }
            else if (!g_ReframeRedo.empty() && !g_App.editor.CanRedo() &&
                     (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) ||
                      ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)))
            {
                g_ReframeUndo.push_back(sv->code);
                sv->code = g_ReframeRedo.back();
                g_ReframeRedo.pop_back();
                ReassembleSurvivor(*sv);
                g_App.editor.SetText(sv->code);
                PushEditorMarkers(*sv);
                g_App.status = "Redid the re-read";
            }
        }
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S)) SaveAll();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))              SaveSurvivor(S.open);

    if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
    {
        if (io.KeyShift)     GameReset();      // rewind the recording
        else if (io.KeyCtrl) GameArm();        // precompute a fresh war
        else                 GamePlayPause();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) GameStepRound(io.KeyShift ? -1 : +1);
    if (ImGui::IsKeyPressed(ImGuiKey_F8,  false)) GameContinue();

    // Alt on the cursor line steps that instruction to its next encoding
    if (ImGui::IsKeyPressed(ImGuiKey_LeftAlt, false) || ImGui::IsKeyPressed(ImGuiKey_RightAlt, false))
        if (S.open.Valid() && !io.KeyCtrl && !io.KeyShift)
            CycleAlternativeOnCursorLine();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !S.selection.empty() && !ImGui::IsAnyItemActive() &&
        !g_Rename.active)
    {
        S.selection.clear();
        S.status = "Selection cleared";
    }
}

// -----------------------------------------------------------------------------
// Fonts
// -----------------------------------------------------------------------------
// Tries each candidate under every known font root. ImFontFlags_NoLoadError
// keeps a missing file from asserting, so a moved/absent fonts folder degrades
// to the built-in font instead of killing the app on startup.
// fonts/ beside a release build, assets/fonts/ beside a development one
static std::vector<std::string> FontDirs()
{
    std::vector<std::string> dirs;
    for (const char* root : { "fonts\\", "assets\\fonts\\" })
    {
        const std::string d = ExeDir() + root;
        const DWORD a = GetFileAttributesA(d.c_str());
        if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) dirs.push_back(d);
    }
    return dirs;
}

// Every .ttf / .otf in the fonts folder, by file name, sorted.
static std::vector<std::string> ListFontFiles()
{
    std::vector<std::string> out;
    for (const std::string& dir : FontDirs())
        for (const char* pat : { "*.ttf", "*.otf" })
        {
            WIN32_FIND_DATAA fd;
            HANDLE h = FindFirstFileA((dir + pat).c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(fd.cFileName);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    std::sort(out.begin(), out.end(), [](const std::string& x, const std::string& y) {
        return ToLower(x) < ToLower(y);
    });
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// "IBMPlexMono-Regular.ttf" -> "IBM Plex Mono": the family part of a Google
// Fonts file name, with its words split back apart.
static std::string FontDisplayName(const std::string& file)
{
    std::string n = file.substr(0, file.find_last_of('.'));
    const size_t dash = n.find('-');
    if (dash != std::string::npos) n = n.substr(0, dash);
    std::string out;
    for (size_t i = 0; i < n.size(); i++)
    {
        const char c = n[i];
        if (i > 0 && isupper((unsigned char)c))
        {
            const char p = n[i - 1];
            const bool nextLower = i + 1 < n.size() && islower((unsigned char)n[i + 1]);
            if (islower((unsigned char)p) || isdigit((unsigned char)p) || (isupper((unsigned char)p) && nextLower))
                out += ' ';
        }
        out += (c == '_') ? ' ' : c;
    }
    return out;
}

// One file from the fonts folder, or NULL if it is not there or will not load.
static ImFont* LoadFontFile(const std::string& file, float sizePx)
{
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;
    cfg.Flags |= ImFontFlags_NoLoadError;
    for (const std::string& dir : FontDirs())
    {
        const std::string path = dir + file;
        if (!FileExists(path)) continue;
        if (ImFont* f = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), sizePx, &cfg))
            return f;
    }
    return NULL;
}

// The first of `candidates` that loads. NoLoadError: a missing or bad file
// falls through to the next one instead of killing the app on startup.
static ImFont* TryLoadFont(const char* const* candidates, int count, float sizePx)
{
    for (int i = 0; i < count; i++)
        if (ImFont* f = LoadFontFile(candidates[i], sizePx)) return f;
    return NULL;
}

static void LoadFonts()
{
    AppState& S = g_App;
    ImGuiIO& io = ImGui::GetIO();

    static const char* uiFonts[] = {
        "Rubik-VariableFont_wght.ttf",
        "GoogleSansCode-VariableFont_MONO,wght.ttf",
        "IBMPlexMono-Regular.ttf"
    };
    static const char* monoFonts[] = {
        "IBMPlexMono-Regular.ttf",
        "DMMono-Regular.ttf",
        "RobotoMono-VariableFont_wght.ttf",
        "SpaceMono-Regular.ttf",
        "CourierPrime-Regular.ttf"
    };

    // the one picked in Options first, then the built-in order
    S.fontUI = S.fontUIFile[0] ? LoadFontFile(S.fontUIFile, S.fontUISize * g_Scale) : NULL;
    if (!S.fontUI) S.fontUI = TryLoadFont(uiFonts, IM_ARRAYSIZE(uiFonts), S.fontUISize * g_Scale);
    if (!S.fontUI) S.fontUI = io.Fonts->AddFontDefault();
    S.fontMono = S.fontMonoFile[0] ? LoadFontFile(S.fontMonoFile, S.fontMonoSize * g_Scale) : NULL;
    if (!S.fontMono) S.fontMono = TryLoadFont(monoFonts, IM_ARRAYSIZE(monoFonts), S.fontMonoSize * g_Scale);
    if (!S.fontMono) S.fontMono = S.fontUI;

    io.FontDefault = S.fontUI;
}

static void ReloadFonts()
{
    AppState& S = g_App;
    ImFont* oldUI = S.fontUI;
    ImFont* oldMono = S.fontMono;
    LoadFonts();
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    if (oldMono && oldMono != oldUI) atlas->RemoveFont(oldMono);
    if (oldUI) atlas->RemoveFont(oldUI);
}

// -----------------------------------------------------------------------------
// Simulation tick  --  advance the live war while it is running
// -----------------------------------------------------------------------------
static void TickSimulation(float dt)
{
    AppState& S = g_App;
    static cw::u64 lastFitted = 0;

    S.game.poll();                                  // adopt a finished recording
    CompetitionPoll();                              // ... and a finished tournament

    // A fresh recording opens the timeline on the stretch where more than one
    // group was still alive -- the rest is a lone survivor running out the
    // clock. Panning and zooming past it is still allowed.
    if (S.game.ready() && S.game.generation() != lastFitted)
    {
        lastFitted = S.game.generation();
        const double total = (double)S.game.totalRounds();
        const double contested = (double)S.game.contestedUntilRound();
        if (contested > 0.0 && contested < total)
        {
            S.tlLo = 0.0;
            S.tlHi = ImMin(total, contested * 1.08 + 8.0);
        }
        else
        {
            S.tlLo = 0.0;                            // never contested: show it all
            S.tlHi = total;
        }
    }

    RefreshBreakPatterns();

    const cw::u64 before = S.game.cycle();
    S.game.tick(dt, RoundsPerSec());                // advance playback
    const cw::u64 after  = S.game.cycle();

    // Stop on the first cycle in the stretch just played that matches a byte
    // pattern, so the break lands on the instruction rather than past it.
    if (S.game.running() && S.game.stopOnEvents && !g_BreakPats.empty() && after != before)
    {
        const cw::Replay& R = S.game.replay();
        const bool fwd = after > before;
        const cw::u64 lo = fwd ? before : after;
        const cw::u64 hi = fwd ? after  : before;
        for (cw::u64 k = 0; k < hi - lo; k++)
        {
            const cw::u64 c = fwd ? (lo + k) : (hi - 1 - k);
            if (!CycleIsBreak(R, c)) continue;
            // stop at the boundary of the round that carried the match, so the
            // matching instruction reads as done
            const int br = R.roundOfCycle(c);
            const size_t nr = R.roundStart.size();
            const cw::u64 stopAt = fwd
                ? ((size_t)(br + 1) < nr ? R.roundStart[(size_t)(br + 1)] : R.cycleCount())
                : R.roundStart[(size_t)br];
            S.game.seekCycle(stopAt);
            S.game.setRunning(false);
            char b[96];
            snprintf(b, sizeof(b), "Breakpoint in round %d", R.roundOfCycle(c));
            S.status = b;
            break;
        }
    }

    if (S.game.ready() || S.game.precomputing())
        S.status = S.game.status();
}

// Files dropped on the window are copied into the workspace -- into the folder
// (or the zombie list) under the mouse, or the selected folder. With no
// workspace open, a dropped folder becomes the workspace.
static void OnDropFiles(GLFWwindow*, int count, const char** paths)
{
    const std::vector<std::string> v(paths, paths + count);
    if (!HasWorkspace())
    {
        if (v.size() == 1 && DirExists(v[0])) { WsOp op; op.kind = WsOp::OpenWorkspace; op.folder = v[0]; QueueWsOp(op); }
        else g_App.status = "Open a workspace first -- or drop a folder here to open it as one";
        return;
    }
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(g_Window, &mx, &my);
    // the drop zones are in ImGui's coordinates: the window's own with one
    // viewport, the desktop's once windows can be popped out
    const ImVec2 vp = ImGui::GetMainViewport()->Pos;
    const ImVec2 at((float)mx + vp.x, (float)my + vp.y);
    WsOp op;
    op.kind = WsOp::Import;
    op.paths = v;
    op.folder = TargetFolder();
    for (const DropZone& z : g_DropZones)
        if (at.x >= z.mn.x && at.y >= z.mn.y && at.x < z.mx.x && at.y < z.mx.y)
        {
            op.folder = z.folder;
            op.zombies = z.zombies;
            break;
        }
    QueueWsOp(op);
}

// -----------------------------------------------------------------------------
// Dock layout
// -----------------------------------------------------------------------------
static void BuildDockLayout(ImGuiID dockId, const ImVec2& size)
{
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, size);

    // Everything shares one node, so the panels come up as tabs of one group
    // instead of being tiled next to each other.
    ImGui::DockBuilderDockWindow("Editor", dockId);
    ImGui::DockBuilderDockWindow("Debug", dockId);
    ImGui::DockBuilderDockWindow("Competition", dockId);
    ImGui::DockBuilderFinish(dockId);
    g_FocusEditorFrames = 2;
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Crash log
//
// A crash appends what happened and the call stack -- as offsets into the exe
// and its DLLs -- to crash.log beside the exe, so "it crashed when I pasted"
// comes with the place it happened (addr2line -e app.exe <0x140000000+offset>).
// -----------------------------------------------------------------------------
static void WriteCrashLog(const char* what, const CONTEXT* ctx)
{
    FILE* f = fopen((ExeDir() + "crash.log").c_str(), "a");
    if (!f) return;
    const time_t now = time(NULL);
    char when[64];
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "=== %s  %s\n", when, what);
    if (ctx)
    {
        CONTEXT c = *ctx;
        for (int i = 0; i < 48 && c.Rip; i++)
        {
            HMODULE mod = NULL;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)c.Rip, &mod);
            char name[MAX_PATH] = "?";
            if (mod) GetModuleFileNameA(mod, name, MAX_PATH);
            fprintf(f, "  #%02d  %s+0x%llx\n", i, FileNameOf(name).c_str(),
                    (unsigned long long)(c.Rip - (DWORD64)(uintptr_t)mod));
            DWORD64 imageBase = 0;
            PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &imageBase, NULL);
            if (fn)
            {
                PVOID handlerData = NULL;
                DWORD64 establisher = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, c.Rip, fn, &c, &handlerData, &establisher, NULL);
            }
            else if (i == 0 && c.Rsp)
            {
                // a leaf function has no unwind data: its return address is on top
                c.Rip = *(const DWORD64*)(uintptr_t)c.Rsp;
                c.Rsp += 8;
            }
            else break;
        }
    }
    fprintf(f, "\n");
    fclose(f);
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep)
{
    char what[128];
    snprintf(what, sizeof(what), "exception 0x%08lX", (unsigned long)ep->ExceptionRecord->ExceptionCode);
    WriteCrashLog(what, ep->ContextRecord);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void TerminateLogger()
{
    std::string what = "std::terminate";
    if (std::exception_ptr p = std::current_exception())
    {
        try { std::rethrow_exception(p); }
        catch (const std::exception& e) { what += std::string(" -- ") + e.what(); }
        catch (...) { what += " -- not a std::exception"; }
    }
    CONTEXT ctx;
    RtlCaptureContext(&ctx);
    WriteCrashLog(what.c_str(), &ctx);
    abort();
}

// Whether any window is out on the desktop on its own (Options > Panels).
static bool PoppedOutWindowsOpen()
{
    if (!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) return false;
    const ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    for (int i = 1; i < pio.Viewports.Size; i++)
        if (!(pio.Viewports[i]->Flags & ImGuiViewportFlags_IsMinimized)) return true;
    return false;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(CrashFilter);
    std::set_terminate(TerminateLogger);
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    GLFWwindow* window = glfwCreateWindow(1360, 840, "Core Wars 8086", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    g_Window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    float sx = 1.0f, sy = 1.0f;
    glfwGetWindowContentScale(window, &sx, &sy);
    g_Scale = (sx > 0.0f) ? sx : 1.0f;

    // --- native frame surgery ------------------------------------------------
    g_Hwnd = glfwGetWin32Window(window);
    // Use the wide variants: GLFW registers a Unicode window class, and an ANSI
    // subclass would make Windows transcode WM_CHAR and break text input.
    g_PrevWndProc = (WNDPROC)SetWindowLongPtrW(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);
    SetWindowPos(g_Hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Centre on the monitor work area before showing anything.
    {
        RECT wr;
        GetWindowRect(g_Hwnd, &wr);
        const int ww = wr.right - wr.left;
        const int wh = wr.bottom - wr.top;
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfo(MonitorFromWindow(g_Hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            const int cx = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - ww) / 2;
            const int cy = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - wh) / 2;
            SetWindowPos(g_Hwnd, NULL, ImMax((LONG)cx, mi.rcWork.left), ImMax((LONG)cy, mi.rcWork.top),
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    glfwShowWindow(window);

    // WM_NCCALCSIZE grew the client area without changing the window rect, so no
    // WM_SIZE was ever sent and the GL drawable is still sized for the old client
    // area - that is what clipped the top of the very first frames. Nudge the
    // window size once to force a real resize.
    {
        RECT wr;
        GetWindowRect(g_Hwnd, &wr);
        const int ww = wr.right - wr.left;
        const int wh = wr.bottom - wr.top;
        SetWindowPos(g_Hwnd, NULL, 0, 0, ww, wh + 1, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SetWindowPos(g_Hwnd, NULL, 0, 0, ww, wh,     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    // --- Dear ImGui ----------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // arrow keys nudge whichever slider has focus; a drag can also be clicked
    // straight into a text box
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigDragClickToInputText = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = false;
    // Separate ini file: the old imgui.ini holds the previous side-by-side layout.
    static const std::string layoutPath = ExeDir() + "corewars_layout.ini";
    io.IniFilename = layoutPath.c_str();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef CWEST_TEST_HOOKS
    // Test builds only: the clipboard reads testclip.txt beside the exe, so a
    // paste can be driven without touching the machine's real clipboard.
    ImGui::GetPlatformIO().Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
        static std::string clip;
        if (!ReadFileBytes(ExeDir() + "testclip.txt", clip)) clip.clear();
        return clip.c_str();
    };
#endif
    ImGui_ImplOpenGL3_Init("#version 130");

    LoadSettings();
    // the offline mode (1) is gone: a setting saved with it is a local run now
    if (g_App.compMode != MODE_SERVER && g_App.compMode != MODE_JOIN) g_App.compMode = MODE_LOCAL;
    if (!g_App.compNetName[0])
        snprintf(g_App.compNetName, sizeof(g_App.compNetName), "%s", NetComputerName().c_str());
    LoadFonts();
    ApplyTheme();
    ImGui::GetStyle().FontSizeBase = g_App.fontUISize * g_Scale;

    g_App.editor.SetPalette(MakeEditorPalette());
    g_App.editor.SetLanguageDefinition(ASM8086());
    g_App.editor.SetTabSize(4);
    g_App.editor.SetShowWhitespaces(false);

    glfwSetDropCallback(window, OnDropFiles);

    // Command line: warrior files and folders as if dropped on the window, and
    // a few switches so a machine can be pointed at a tournament from a
    // shortcut without anyone clicking through the panel on it.
    //
    //   --folder <dir>        open this workspace instead of the last one
    //   --serve [port]        host: listen, and Run the tournament straight away
    //   --join host[:port]    play a share of the tournament at host
    //   --games <n>           battles for the run (0 = until Stop)
    //   --threads <n>         worker threads for this machine (0 = every core)
    //   --engine c++|java|busy which lanes this machine plays with (busy only when allowed in Options)
    //   --mode local|server|join
    //   --run                 press Run once the field is loaded
    bool autoRun = false;
    bool joinAtStart = false;      // --join dials once every option is read (--password may follow it)
    {
        std::vector<std::string> a;
        for (int i = 1; i < argc; i++)
        {
            const std::string arg = argv[i];
            const bool more = (i + 1 < argc);
            if (arg == "--folder" && more)      { g_StartWorkspace = argv[++i]; continue; }
            if (arg == "--games" && more)       { g_App.compGames = atoi(argv[++i]); continue; }
            if (arg == "--threads" && more)     { g_App.compThreads = atoi(argv[++i]); continue; }
            if (arg == "--mode" && more)
            {
                const std::string m = ToLower(argv[++i]);
                g_App.compMode = (m == "server") ? MODE_SERVER : (m == "join") ? MODE_JOIN : MODE_LOCAL;
                continue;
            }
            if (arg == "--run")                 { autoRun = true; continue; }
            if (arg == "--engine" && more)
            {
                const std::string e = ToLower(argv[++i]);
                g_App.compBackend = ((e == "busy" || e == "both") && g_App.compAllowBusy) ? COMP_BOTH
                                  : (e == "java") ? COMP_JAVA : COMP_CPU;
                continue;
            }
            if (arg == "--serve")
            {
                if (more && argv[i + 1][0] != '-') g_App.compNetPort = atoi(argv[++i]);
                g_App.compMode = MODE_SERVER;
                std::string why;
                if (!NetServerStart(g_App.compNetPort, why)) g_CompNetNote = why;
                autoRun = true;
                continue;
            }
            if (arg == "--join" && more)
            {
                // the host's https address, or host:port on this machine
                snprintf(g_App.compNetHost, sizeof(g_App.compNetHost), "%s", argv[++i]);
                g_App.compMode = MODE_JOIN;
                g_App.showBoard = true;
                g_CompRaise = true;
                joinAtStart = true;
                continue;
            }
            if (arg == "--password" && more)
            {
                snprintf(g_App.compNetPassword, sizeof(g_App.compNetPassword), "%s", argv[++i]);
                continue;
            }
            a.push_back(argv[i]);
        }
        // a lone folder on the command line is the workspace to open
        if (a.size() == 1 && DirExists(a[0])) { g_StartWorkspace = a[0]; a.clear(); }
        if (!g_StartWorkspace.empty() && DirExists(g_StartWorkspace))
        {
            const double t0 = glfwGetTime();
            OpenWorkspace(g_StartWorkspace);
#ifdef CWEST_TEST_HOOKS
            char b[96];
            snprintf(b, sizeof(b), "open %.1f ms, %d groups, %d folders\n", (glfwGetTime() - t0) * 1000.0,
                     (int)g_App.players.size(), (int)g_App.folders.size());
            std::string all = b;
            for (auto& kv : g_Perf)
            {
                char l[96];
                snprintf(l, sizeof(l), "  %-10s %8.1f ms", kv.first.c_str(), kv.second.sum);
                all += l;
                all += (char)10;
            }
            WriteFileBytes(ExeDir() + "perf_open.txt", all);
#else
            (void)t0;
#endif
        }
        // anything else comes into the workspace as if dropped on it
        if (!a.empty() && HasWorkspace()) ImportIntoWorkspace(a, std::string(), false);
    }
    if (joinAtStart)
    {
        std::string why;
        if (!NetClientConnect(g_App.compNetHost, why)) { g_NetClient.state = NET_FAILED; g_NetClient.note = why; }
    }
    if (autoRun) { g_App.showBoard = true; g_CompRaise = true; CompetitionStart(); }

    g_AppReady = true;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        // Windows popped out of the main one stay on the desktop when it is
        // minimized (they are not owned by it), so they are kept drawing --
        // paced by hand, since a minimized window's swap does not wait for
        // the screen.
        const bool iconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
        if (iconified && PoppedOutWindowsOpen())
        {
            AppFrame();
            Sleep(15);
            continue;
        }
        if (iconified)
        {
            // No drawing while minimized, but a tournament must not stall: a
            // host keeps accepting joiners and merging their packs, a joiner
            // keeps knocking, picks up its field and reports back.
            ArenaMouseUnlock();
            CompetitionPoll();
            Sleep(10);
            continue;
        }
        AppFrame();
    }
    g_AppReady = false;
    ArenaMouseUnlock();

    FlushEditorToModel();
    if (g_App.autoSave) SaveDirty();   // the last few keystrokes, before the pause
    WriteWsStateNow();
    StopWatch();
    SaveSettings();
    SeedSearchStop();
    SmartCheckStop();
    CompetitionStop();
    if (g_CompThread && g_CompThread->joinable()) g_CompThread->join();
    GpuLaneStop();
    JavaLaneStop();
    NetServerStop();
    NetClientDisconnect();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#ifdef CWEST_TEST_HOOKS
static void PerfTick()
{
    static double next = 0.0;
    const double now = glfwGetTime();
    if (now < next) return;
    next = now + 3.0;
    std::string out;
    char line[160];
    for (auto& kv : g_Perf)
    {
        snprintf(line, sizeof(line), "%-12s avg %7.3f ms  max %7.3f ms  (%d)\n", kv.first.c_str(),
                 kv.second.n ? kv.second.sum / kv.second.n : 0.0, kv.second.max, kv.second.n);
        out += line;
    }
    WriteFileBytes(ExeDir() + "perf.txt", out);
    g_Perf.clear();
}

// Test builds only: a file shot.req beside the exe, holding a path, makes the
// next frame save the main window's back buffer there as a .bmp. Read straight
// from GL, so it works while the screen is locked and PrintWindow sees black.
// shot.txt beside it gets a few facts a picture cannot show.
static void SaveBackBufferBmp(const std::string& out, int fbW, int fbH)
{
    if (out.empty() || fbW <= 0 || fbH <= 0) return;
    std::vector<unsigned char> px((size_t)fbW * fbH * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    const int rowBytes = (fbW * 3 + 3) & ~3;
    std::string bmp(54 + (size_t)rowBytes * fbH, '\0');
    auto put32 = [&](size_t at, uint32_t v) { for (int k = 0; k < 4; k++) bmp[at + k] = (char)((v >> (8 * k)) & 0xFF); };
    bmp[0] = 'B'; bmp[1] = 'M';
    put32(2, (uint32_t)bmp.size()); put32(10, 54); put32(14, 40);
    put32(18, (uint32_t)fbW); put32(22, (uint32_t)fbH);
    bmp[26] = 1; bmp[28] = 24;
    put32(34, (uint32_t)(rowBytes * fbH));
    for (int y = 0; y < fbH; y++)                 // GL's rows run bottom-up, as a BMP's do
        for (int x = 0; x < fbW; x++)
        {
            const unsigned char* s = &px[((size_t)y * fbW + x) * 4];
            char* d = &bmp[54 + (size_t)y * rowBytes + (size_t)x * 3];
            d[0] = (char)s[2]; d[1] = (char)s[1]; d[2] = (char)s[0];
        }
    WriteFileBytes(out, bmp);
}

static std::string TakeShotRequest(const char* file)
{
    const std::string req = ExeDir() + file;
    std::string out;
    if (!ReadFileBytes(req, out)) return std::string();
    DeleteFileA(req.c_str());
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
    return out;
}

// shotvp.req does the same for every window popped out of the main one
// (<path>_1.bmp, _2 ...): it renders them itself this frame, reading each
// back buffer before it is swapped. Returns whether it did.
static bool TestShotViewports()
{
    const std::string out = TakeShotRequest("shotvp.req");
    if (out.empty()) return false;
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    std::string log;
    for (int i = 0; i < pio.Viewports.Size; i++)
    {
        int w = 0, h = 0;
        glfwGetFramebufferSize((GLFWwindow*)pio.Viewports[i]->PlatformHandle, &w, &h);
        char line[160];
        snprintf(line, sizeof(line), "pio viewport %d id %08X flags %08X fb %dx%d\n", i, pio.Viewports[i]->ID,
                 (unsigned)pio.Viewports[i]->Flags, w, h);
        log += line;
    }
    WriteFileBytes(ExeDir() + "shotvp.txt", log);
    for (int i = 1; i < pio.Viewports.Size; i++)
    {
        ImGuiViewport* vp = pio.Viewports[i];
        if (vp->Flags & ImGuiViewportFlags_IsMinimized) continue;
        if (pio.Platform_RenderWindow) pio.Platform_RenderWindow(vp, NULL);
        if (pio.Renderer_RenderWindow) pio.Renderer_RenderWindow(vp, NULL);
        int w = 0, h = 0;
        glfwGetFramebufferSize((GLFWwindow*)vp->PlatformHandle, &w, &h);
        SaveBackBufferBmp(out + "_" + std::to_string(i) + ".bmp", w, h);
        if (pio.Platform_SwapBuffers) pio.Platform_SwapBuffers(vp, NULL);
        if (pio.Renderer_SwapBuffers) pio.Renderer_SwapBuffers(vp, NULL);
    }
    return true;
}

static void TestShotTick(int fbW, int fbH)
{
    const std::string out = TakeShotRequest("shot.req");
    if (out.empty()) return;
    SaveBackBufferBmp(out, fbW, fbH);
    std::string facts;
    char line[200];
    ImGuiContext& g = *GImGui;
    snprintf(line, sizeof(line), "frame %d  viewports %d  enabled %d\n", g_FrameNo, g.Viewports.Size,
             (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ? 1 : 0);
    facts += line;
    for (ImGuiViewportP* v : g.Viewports)
    {
        snprintf(line, sizeof(line), "viewport %08X pos %.0f,%.0f size %.0fx%.0f window %s platform %d\n",
                 v->ID, v->Pos.x, v->Pos.y, v->Size.x, v->Size.y, v->Window ? v->Window->Name : "-",
                 v->PlatformWindowCreated ? 1 : 0);
        facts += line;
    }
    snprintf(line, sizeof(line), "edit open %d reg %d  pokes %d  status %s\n", g_Edit.open ? 1 : 0,
             g_Edit.isReg ? 1 : 0, (int)g_App.pokes.size(), g_App.status.c_str());
    facts += line;
    WriteFileBytes(ExeDir() + "shot.txt", facts);
}
#else
static void PerfTick() {}
#endif

// One whole frame: build the UI, draw it, present it. Called from the main
// loop, and also from inside the window procedure while Windows is running its
// own modal loop for a resize or a move -- without that the last frame was
// simply stretched to the new size until the mouse was let go.
static void AppFrame()
{
    if (g_InFrame) return;
    g_InFrame = true;
    g_FrameNo++;
    PerfTick();
    GLFWwindow* window = g_Window;
    ImGuiIO& io = ImGui::GetIO();
    bool& layoutBuilt = g_LayoutBuilt;
    {
        if (g_FontsDirty)
        {
            g_FontsDirty = false;
            ReloadFonts();
        }

        // Windows that may leave the main one (Options > Appearance > Panels):
        // Dear ImGui's multi-viewports, flipped between frames -- it moves
        // every window across to the new coordinates itself when it changes.
        if (g_App.popOutWindows) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        else                     io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

        const double perfT0 = glfwGetTime();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        g_NoDragBuild.clear();
        BeginLongPressFrame();
        TickSimulation(io.DeltaTime);

        AppState& S = g_App;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        const float titleH   = ImFloor(42.0f * g_Scale);
        const float statusH  = ImFloor(26.0f * g_Scale);
        const float splitW   = ImFloor(5.0f * g_Scale);
        const float menuPadY = ImMax(1.0f, (titleH - ImGui::GetFontSize()) * 0.5f);

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        const ImGuiWindowFlags rootFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f * g_Scale, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11.0f * g_Scale, menuPadY));
        ImGui::Begin("CoreWarsRoot", NULL, rootFlags);

        g_TitleBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight;
        if (ImGui::BeginMenuBar())
        {
            DrawTitleBar();
            ImGui::EndMenuBar();
        }
        ImGui::PopStyleVar(4);

        const ImVec2 rootMin = ImGui::GetCursorScreenPos();
        const float  rootW   = ImGui::GetContentRegionAvail().x;
        const float  rootH   = ImGui::GetContentRegionAvail().y;

        // The strip under the title bar is gone -- every control it held lives on
        // the panel it belongs to, or in the options window.
        const float bodyTop = rootMin.y;
        const float bodyH   = ImMax(60.0f, rootH - statusH);

        // collapsed, the strip is only as wide as two survivor chips
        const float wsW = S.groupsCollapsed
            ? CollapsedGroupsWidth()
            : ImClamp(S.workspaceWidth, 190.0f * g_Scale, ImMax(200.0f * g_Scale, rootW - 360.0f * g_Scale));
        if (!S.groupsCollapsed) S.workspaceWidth = wsW;

        ImGui::SetCursorScreenPos(ImVec2(rootMin.x, bodyTop));
        { PERF("groups"); DrawWorkspacePanel(wsW, bodyH); }

        // splitter
        {
            ImGui::SetCursorScreenPos(ImVec2(rootMin.x + wsW, bodyTop));
            BtnState sp = BareButton("##splitter", ImVec2(splitW, bodyH));
            // collapsed, the strip has a fixed width and there is nothing to drag
            const bool canDrag = !S.groupsCollapsed;
            if (sp.held && canDrag) S.workspaceWidth += ImGui::GetIO().MouseDelta.x;
            if (canDrag && (sp.hovered || sp.held)) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            ImGui::GetWindowDrawList()->AddRectFilled(
                sp.mn, sp.mx, HexU((sp.hovered || sp.held) ? Col::Accent : Col::Line, (sp.hovered || sp.held) ? 0.85f : 1.0f));
        }

        // dockspace: one node, every panel tabbed into it
        const ImGuiID dockId = ImGui::GetID("CoreWarsDockSpace");
        // placed from the strip's drawn width, not the saved one: collapsed,
        // the two differ and the panels have to grow into the space it gave up
        const ImVec2 dockPos(rootMin.x + wsW + splitW, bodyTop);
        const ImVec2 dockSize(ImMax(60.0f, rootW - wsW - splitW), bodyH);
        // Whether the layout file had a node for this, asked before DockSpace()
        // -- which creates the node -- or a first run never gets its tabs.
        const bool noSavedLayout = !layoutBuilt && ImGui::DockBuilderGetNode(dockId) == NULL;
        ImGui::SetCursorScreenPos(dockPos);
        ImGui::DockSpace(dockId, dockSize, ImGuiDockNodeFlags_NoWindowMenuButton);

        if (!layoutBuilt || g_ResetLayout)
        {
            if (g_ResetLayout || noSavedLayout)
                BuildDockLayout(dockId, dockSize);
            layoutBuilt = true;
            g_ResetLayout = false;
        }

        ImGui::SetCursorScreenPos(ImVec2(rootMin.x, bodyTop + bodyH));
        DrawStatusBar(statusH);

        ImGui::End(); // CoreWarsRoot

        // Docked panels are submitted after the host window is closed.
        if (S.showEditor) { PERF("editor"); DrawEditorPanel(); }
        if (S.showArena)  { PERF("arena"); DrawArenaPanel(); }
        if (S.showBoard)  { PERF("board"); DrawLeaderboardPanel(); }
        DrawPoppedInspectors(S.game);
        DrawAboutPopup();
        EditFieldSweep();
        DrawOptionsWindow();
        DrawSeedSearchWindow();
        DrawSmartCheckWindow();
        DrawRunFoldersWindow();
        DrawExportWindow();

        { PERF("shortcuts"); HandleShortcuts(); }
        { PERF("wsops"); RunWsOps(); }
        { PERF("autosave"); AutoSaveTick(); }
        { PERF("wsstate"); WsStateTick(); }
        { PERF("watch"); WsWatchTick(); }
        KeepFloatingWindowsClickable();

        // --- render ----------------------------------------------------------
        const double perfR0 = glfwGetTime();
        ImGui::Render();
        g_NoDragActive.swap(g_NoDragBuild);
        if (g_MouseLockFrame >= 0 && g_MouseLockFrame != g_FrameNo) ArenaMouseUnlock();   // the arena went away mid-zoom

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        const ImVec4 clear = Hex(Col::Bg0);
        glClearColor(clear.x, clear.y, clear.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#ifdef CWEST_TEST_HOOKS
        TestShotTick(fbW, fbH);
#endif
        // and the windows dragged out of it, each into its own. Updated every
        // frame, on or off: ImGui checks the last frame did it before it lets
        // the option be switched on, and it does nothing while off.
        {
            GLFWwindow* ctx = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
#ifdef CWEST_TEST_HOOKS
            if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) && !TestShotViewports())
#else
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
#endif
                ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(ctx);
        }
        PerfAdd("render", perfR0);
        PerfAdd("frame", perfT0);

        glfwSwapBuffers(window);
    }
    g_InFrame = false;
}

// A floating ImGui window (Options, a popped-out inspector, an undocked panel)
// can be dragged up over the app's own title strip. The strip answers
// HTCAPTION there, so Windows took the click and dragged the whole app instead
// of the panel -- and the panel could not be got back down. Wherever such a
// window covers the strip, the strip is client area.
static void KeepFloatingWindowsClickable()
{
    ImGuiContext& g = *GImGui;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float stripBottom = vp->Pos.y + g_TitleBarHeight;
    for (ImGuiWindow* w : g.Windows)
    {
        if (!w->Active || w->Hidden) continue;
        if (w->Flags & ImGuiWindowFlags_ChildWindow) continue;
        if (w->DockIsActive) continue;                           // docked: below the strip
        if (w->Viewport != vp) continue;                         // out on the desktop, a window of its own
        if (strcmp(w->Name, "CoreWarsRoot") == 0) continue;      // the strip itself
        if (w->Pos.y >= stripBottom) continue;
        AddNoDragRect(w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y));
    }
}
