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
#include <memory>
#include <thread>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
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
namespace Col {
    inline unsigned Bg0     = 0x0E1014;   // app background
    inline unsigned Bg1     = 0x14171D;   // panels
    inline unsigned Bg2     = 0x1A1E26;   // frames / elevated
    inline unsigned Bg3     = 0x232936;   // hovered
    inline unsigned Bg4     = 0x2C3341;   // active
    inline unsigned Line    = 0x262C38;   // borders / separators
    inline unsigned Text    = 0xD7DEE9;
    inline unsigned TextDim = 0x76818F;
    inline unsigned Accent  = 0x4C8DFF;
    inline unsigned Accent2 = 0x2FD4C0;
    inline unsigned Warn    = 0xF2B549;
    inline unsigned Danger  = 0xE5484D;
    inline unsigned Ok      = 0x53C08A;
}

// One slot per colour above, in the order the customizer lists them.
static const int kPalCount = 13;
static unsigned* PalSlot(int i)
{
    unsigned* t[kPalCount] = { &Col::Bg0, &Col::Bg1, &Col::Bg2, &Col::Bg3, &Col::Bg4,
                               &Col::Line, &Col::Text, &Col::TextDim,
                               &Col::Accent, &Col::Accent2, &Col::Warn, &Col::Danger, &Col::Ok };
    return t[i];
}
static const char* PalName(int i)
{
    static const char* n[kPalCount] = { "Background", "Panel", "Frame", "Hovered", "Active",
                                        "Border", "Text", "Dim text",
                                        "Accent", "Accent 2", "Warning", "Danger", "OK" };
    return n[i];
}

struct ThemeDef { const char* name; unsigned c[kPalCount]; };
static const ThemeDef kThemes[] = {
    { "Midnight (dark)", { 0x0E1014, 0x14171D, 0x1A1E26, 0x232936, 0x2C3341, 0x262C38,
                           0xD7DEE9, 0x76818F, 0x4C8DFF, 0x2FD4C0, 0xF2B549, 0xE5484D, 0x53C08A } },
    { "Paper (light)",   { 0xF3F4F7, 0xFFFFFF, 0xECEEF3, 0xDFE3EC, 0xCED4E1, 0xD3D8E2,
                           0x1B1F27, 0x6B7280, 0x2563EB, 0x0D9488, 0xB45309, 0xDC2626, 0x15803D } },
    { "Amber CRT",       { 0x120E08, 0x1A150C, 0x231C10, 0x2F2618, 0x3B3020, 0x33291A,
                           0xF0D9A8, 0x9A835C, 0xFFB000, 0x7FD1AE, 0xFFD166, 0xFF5C4D, 0x8BD450 } },
    { "Slate",           { 0x14161A, 0x1C1F26, 0x242830, 0x2E333D, 0x3A404C, 0x30353F,
                           0xE2E6EC, 0x848C99, 0x7C93FF, 0x59D0B0, 0xE8C15C, 0xEE5A5F, 0x66C48C } },
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
    std::string name;      // display name (file name)
    std::string path;      // on-disk path, empty for scratch buffers
    std::string code;      // assembly source (editor buffer)
    std::vector<unsigned char> bin;   // assembled machine code; non-empty => runnable
    std::vector<cw::AsmLine> asmLines;   // source line -> byte range
    std::vector<cw::AsmDiag> asmDiags;
    cw::Listing  listing;                // as first generated, for the warn flags
    bool        asmOk = true;
    bool        present = false;
    bool        enabled = true;
    bool        dirty   = false;
    bool        runnable() const { return present && enabled && !bin.empty(); }
};

struct Player
{
    std::string name;
    Survivor    slots[kSlotCount];
    bool        enabled = true;
    unsigned    tint = Col::Accent;
    // With twoTone off, survivor 2 is a lightened tint; with it on, tint2 is
    // its own colour and the two survivors read as separate warriors.
    unsigned    tint2 = Col::Accent;
    bool        twoTone = false;

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

// A row in the workspace. slot == -1 means the player row itself.
struct Ref
{
    int player = -1;
    int slot   = -2;
    bool Valid() const
    {
        if (player == -2) return slot >= 0;             // kZombiePlayer
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

struct AppState
{
    std::vector<Player> players;
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
    bool                showAbout = false, showDemo = false;
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
    bool                arenaBgCustom = false;
    unsigned            arenaBg = 0x0E1014;
    bool                advanceSeed = false;      // bump the seed on every recompute
    // --- competition ---
    cw::CompeteResult   compResult;
    cw::CompeteProgress compProg;
    bool                compRunning = false;
    int                 compGames   = 200;    // 0 = limitless
    int                 compThreads = 0;      // 0 = every core
    int                 compByGroup = 1;      // rank groups, or their survivors
    int                 compBackend = 0;      // 0 CPU, 1 GPU, 2 both lanes, 3 Java
    std::string         compGpuNote;          // why the GPU or Java lane is not running
    // Groups drawn into each battle. A war holds twenty warriors, so a field
    // bigger than about nine groups cannot all fight at once; four is what the
    // reference tournament uses. 0 means "the whole field".
    int                 compCombo   = 4;
    int                 compPairSeed = 20260731;   // which combinations come up
    // How the tournament is shared out: 0 this machine, 1 shard files carried
    // between machines, 2 host a run other machines join, 3 join one.
    int                 compMode    = 0;
    int                 compNetPort = 8086;
    char                compNetHost[128] = "127.0.0.1";
    char                compNetName[64]  = "";     // filled from the computer name
    int                 compShards  = 4;           // shard files an export writes
    // The bar graph: columns rising from the floor instead of bars from the
    // left, and squeezed to fit the panel (3 px a bar at most) with the stats
    // on hover, the way the timeline lanes work.
    bool                compBarsUp  = false;
    bool                compBarsFit = false;

    // The edit tool: staged changes and whether a re-record is owed.
    bool                editMode = false;
    std::vector<cw::Poke> pokes;
    bool                pokesPending = false;
    int                 pokeRound = 0;

    std::vector<Zombie> zombies;

    // --- options -----------------------------------------------------------
    bool                showOptions = false;
    int                 themeIndex  = 0;
    // folder flow
    std::string         lastFolder;
    int                 folderSortMode = 0;      // 0 ask, 1 always sort, 2 never
    bool                folderAssemble = true;   // assembly/*.asm  -> survivors/
    bool                folderDisassemble = true;// survivors/*     -> assembly/
    // breakpoints
    char                breakPatterns[128] = "";  // "CD 20, F4" -- hex, comma separated
    bool                breakStartsWith = false;  // false: bytes appear anywhere in the instruction
    bool                breakOnTimeline = true;
    // Pointer emphasis: the arena cell each of these registers points at gets a
    // ring in its own colour. The segment half is fixed in practice, so only the
    // offset matters -- see the note above ArenaPointerRings.
    bool                hlBase = true;      // BX, BP
    bool                hlIndex = true;     // SI, DI
    bool                hlPointer = true;   // SP
    unsigned            hlColor = 0xF2B549;

    std::string         status = "Ready";
    ImFont*             fontUI = nullptr;
    ImFont*             fontMono = nullptr;
    float               fontUISize = 16.0f;
    float               fontMonoSize = 15.0f;
    float               fontEditorSize = 15.0f;   // the source editor alone (Ctrl +/-)
};
static AppState g_App;

// Defined in gui_options.inl, but reached from the menus and the toolbar.
static void OpenWorkspaceFolder();
static void LoadWorkspaceFolder(const std::string& root);
static void ImportLooseFiles();
static void DrawZombieSection();
static void ZombieChip(int zi, float size);
static bool SaveBinarySurvivor(Survivor& sv);
static void DrawOptionsWindow();

// Set by the timeline when its lanes need more height than the default
// strip gives them; read here on the next frame. 0 means the default.
static float g_TimelineWantH = 0.0f;

static float       g_Scale = 1.0f;   // DPI / UI scale
static HWND        g_Hwnd = nullptr;
static GLFWwindow* g_Window = nullptr;

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

#include "gui_editor.inl"   // listing / re-assembly / assembled preview

// TextEditor::GetText() always appends one extra trailing newline; drop it so a
// freshly opened buffer does not immediately look modified.
static std::string EditorText()
{
    std::string t = g_App.editor.GetText();
    if (!t.empty() && t[t.size() - 1] == '\n') t.erase(t.size() - 1);
    return t;
}

static void FlushEditorToModel()
{
    Survivor* s = SurvivorAt(g_App.open);
    if (!s || !s->present) return;
    const std::string text = EditorText();
    if (text != s->code)
    {
        s->code = text;
        s->dirty = true;
        ReassembleSurvivor(*s);           // gutter + diagnostics follow the text
    }
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
    PushEditorMarkers(*s);
    g_App.editor.SetCursorPosition(TextEditor::Coordinates(0, 0));
    g_App.status = "Opened " + OwnerNameOf(target) + " / " + s->name;
}

static void SaveSurvivor(const Ref& r)
{
    if (r == g_App.open) FlushEditorToModel();
    Survivor* s = SurvivorAt(r);
    if (!s || !s->present || s->path.empty()) return;
    // A survivor loaded from a binary has no source file yet -- writing the
    // editor text straight over the machine code would destroy it, so that case
    // goes through the folder-aware saver instead.
    if (SaveBinarySurvivor(*s)) return;
    if (WriteTextFile(s->path, s->code))
    {
        s->dirty = false;
        g_App.status = "Saved " + s->name;
    }
    else
    {
        g_App.status = "FAILED to save " + s->name;
    }
}

static void SaveAll()
{
    FlushEditorToModel();
    int n = 0;
    // One writer for both kinds of row, so a binary-backed survivor never has
    // its machine code overwritten with editor text.
    auto saveOne = [&n](Survivor& sv) {
        if (!sv.present || !sv.dirty || sv.path.empty()) return;
        if (SaveBinarySurvivor(sv)) { if (!sv.dirty) n++; return; }
        if (WriteTextFile(sv.path, sv.code)) { sv.dirty = false; n++; }
    };
    for (int p = 0; p < (int)g_App.players.size(); p++)
        for (int s = 0; s < kSlotCount; s++)
            saveOne(g_App.players[p].slots[s]);
    for (Zombie& z : g_App.zombies)
        saveOne(z.sv);
    char buf[64];
    snprintf(buf, sizeof(buf), "Saved %d file(s)", n);
    g_App.status = buf;
}

// -----------------------------------------------------------------------------
// Selection
// -----------------------------------------------------------------------------
static std::vector<Ref> FlatRows()
{
    std::vector<Ref> rows;
    for (int p = 0; p < (int)g_App.players.size(); p++)
    {
        Ref r; r.player = p; r.slot = -1; rows.push_back(r);
        for (int s = 0; s < kSlotCount; s++)
        {
            Ref q; q.player = p; q.slot = s; rows.push_back(q);
        }
    }
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
    const char* state = RefEnabled(r) ? " enabled" : " disabled";
    if (IsZombieRef(r))
    {
        g_App.status = g_App.zombies[r.slot].name + state;
        return;
    }
    if (r.player < 0 || r.player >= (int)g_App.players.size()) return;
    const Player& p = g_App.players[r.player];
    g_App.status = (r.slot == -1) ? (p.name + state)
                                  : (p.name + " / " + p.slots[r.slot].name + state);
}

// Ctrl+Q: disable everything selected. If it is all disabled already, re-enable.
// Delete on the groups selection: a survivor row empties its slot, a group row
// closes the whole group, a zombie row is removed outright.
static void DeleteSelection()
{
    if (g_App.selection.empty()) { g_App.status = "Nothing selected"; return; }

    // Work back to front so erasing does not shift the refs still to come.
    std::vector<Ref> sel = g_App.selection;
    std::sort(sel.begin(), sel.end(), [](const Ref& a, const Ref& b) {
        if (a.player != b.player) return a.player > b.player;
        return a.slot > b.slot;
    });

    int n = 0;
    for (const Ref& r : sel)
    {
        if (IsZombieRef(r))
        {
            if (r.slot < (int)g_App.zombies.size())
            { g_App.zombies.erase(g_App.zombies.begin() + r.slot); n++; }
        }
        else if (r.player >= 0 && r.player < (int)g_App.players.size())
        {
            if (r.slot == -1) { g_App.players.erase(g_App.players.begin() + r.player); n++; }
            else if (r.slot >= 0 && r.slot < kSlotCount && g_App.players[r.player].slots[r.slot].present)
            { g_App.players[r.player].slots[r.slot] = Survivor(); n++; }
        }
    }

    g_App.selection.clear();
    g_App.anchor = Ref();
    g_App.open = Ref();
    g_App.editor.SetText("");
    g_App.editor.SetReadOnly(true);
    char buf[64];
    snprintf(buf, sizeof(buf), "Closed %d item(s)", n);
    g_App.status = buf;
}

static void ToggleSelectionEnabled()
{
    if (g_App.selection.empty()) { g_App.status = "Nothing selected"; return; }
    bool allDisabled = true;
    for (size_t i = 0; i < g_App.selection.size(); i++)
        if (RefEnabled(g_App.selection[i])) { allDisabled = false; break; }

    for (size_t i = 0; i < g_App.selection.size(); i++)
        SetRefEnabled(g_App.selection[i], allDisabled);

    char buf[96];
    snprintf(buf, sizeof(buf), "%s %d item(s)", allDisabled ? "Enabled" : "Disabled", (int)g_App.selection.size());
    g_App.status = buf;
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

static int FindOrCreatePlayer(const std::string& name)
{
    const std::string key = ToLower(name);
    for (int i = 0; i < (int)g_App.players.size(); i++)
        if (ToLower(g_App.players[i].name) == key) return i;

    Player p;
    p.name = name;
    p.tint = kPlayerTints[g_App.players.size() % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
    g_App.players.push_back(p);
    return (int)g_App.players.size() - 1;
}

static void LoadSurvivorFiles(const std::vector<std::string>& paths)
{
    int loaded = 0;
    Ref lastLoaded;

    for (size_t i = 0; i < paths.size(); i++)
    {
        std::string text;
        if (!ReadTextFile(paths[i], text))
        {
            g_App.status = "Could not read " + FileNameOf(paths[i]);
            continue;
        }

        const std::string fileName = FileNameOf(paths[i]);
        std::string base; int index = 0;
        SplitIndexedStem(StemOf(fileName), base, index);

        int pi = FindOrCreatePlayer(base);

        // survivor1.asm -> slot 0, survivor2.asm -> slot 1, anything else -> first free
        int slot = (index >= 1 && index <= kSlotCount) ? index - 1 : -1;
        if (slot < 0 || g_App.players[pi].slots[slot].present)
        {
            slot = -1;
            for (int s = 0; s < kSlotCount; s++)
                if (!g_App.players[pi].slots[s].present) { slot = s; break; }
        }
        if (slot < 0)
        {
            // both slots taken: start an overflow player so nothing is silently dropped
            pi = FindOrCreatePlayer(base + " (2)");
            slot = 0;
            for (int s = 0; s < kSlotCount; s++)
                if (!g_App.players[pi].slots[s].present) { slot = s; break; }
        }

        Survivor& sv = g_App.players[pi].slots[slot];
        sv.name    = fileName;
        sv.path    = paths[i];
        sv.code    = text;
        sv.present = true;
        sv.enabled = true;
        sv.dirty   = false;

        lastLoaded.player = pi;
        lastLoaded.slot   = slot;
        loaded++;
    }

    if (loaded > 0)
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "Loaded %d survivor file(s)", loaded);
        g_App.status = buf;
        g_App.selection.assign(1, lastLoaded);
        g_App.anchor = lastLoaded;
        OpenInEditor(lastLoaded);
    }
}

// -----------------------------------------------------------------------------
// Binary (machine-code) survivors  --  these are what the engine actually runs.
// Same trailing-number pairing as the text loader; the assembly buffer is left
// as a hex dump until the real disassembler lands (Phase 5).
// -----------------------------------------------------------------------------
static bool ReadBinaryFile(const std::string& path, std::vector<unsigned char>& out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

static void LoadBinarySurvivorFiles(const std::vector<std::string>& paths)
{
    int loaded = 0;
    Ref last;
    for (const std::string& path : paths)
    {
        std::vector<unsigned char> bytes;
        if (!ReadBinaryFile(path, bytes)) { g_App.status = "Could not read " + FileNameOf(path); continue; }
        if (bytes.size() > 512) bytes.resize(512);   // WarriorRepository.MAX_WARRIOR_SIZE

        const std::string fileName = FileNameOf(path);
        std::string base; int index = 0;
        SplitIndexedStem(StemOf(fileName), base, index);

        int pi = FindOrCreatePlayer(base);
        int slot = (index >= 1 && index <= kSlotCount) ? index - 1 : -1;
        if (slot < 0 || g_App.players[pi].slots[slot].present)
        {
            slot = -1;
            for (int s = 0; s < kSlotCount; s++)
                if (!g_App.players[pi].slots[s].present) { slot = s; break; }
        }
        if (slot < 0)
        {
            pi = FindOrCreatePlayer(base + " (2)");
            slot = 0;
        }

        Survivor& sv = g_App.players[pi].slots[slot];
        sv.name    = fileName;
        sv.path    = path;
        ListSurvivor(sv, bytes);          // real disassembly, labels and all
        sv.present = true;
        sv.enabled = true;
        sv.dirty   = false;
        last.player = pi; last.slot = slot;
        loaded++;
    }
    if (loaded > 0)
    {
        g_App.status = "Loaded " + std::to_string(loaded) + " binary survivor(s)";
        g_App.selection.assign(1, last);
        g_App.anchor = last;
        OpenInEditor(last);
    }
}

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

static std::vector<cw::WarriorGroup> BuildGroups()
{
    std::vector<cw::WarriorGroup> groups;
    for (const Player& pl : g_App.players)
    {
        if (!pl.enabled) continue;
        cw::WarriorGroup g;
        g.name = pl.name;
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
        if (!g.warriors.empty()) groups.push_back(std::move(g));
    }

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
        g_App.status = "Need at least one group with an assembled survivor";
        return;
    }
    g_App.tlLo = 0.0; g_App.tlHi = 0.0;      // timeline re-fits once the replay lands
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

static void NewPlayer()
{
    char name[64];
    snprintf(name, sizeof(name), "Player %d", (int)g_App.players.size() + 1);
    int pi = FindOrCreatePlayer(name);
    for (int s = 0; s < kSlotCount; s++)
    {
        Survivor& sv = g_App.players[pi].slots[s];
        char fn[64];
        snprintf(fn, sizeof(fn), "survivor%d.asm", s + 1);
        sv.name    = fn;
        sv.path    = "";
        sv.code    = kTemplate;
        sv.present = true;
        sv.enabled = true;
        sv.dirty   = true;
    }
    Ref r; r.player = pi; r.slot = 0;
    g_App.selection.assign(1, r);
    g_App.anchor = r;
    OpenInEditor(r);
    g_App.status = std::string("Created ") + name;
}

static void ClosePlayer(int pi)
{
    if (pi < 0 || pi >= (int)g_App.players.size()) return;
    g_App.status = "Closed " + g_App.players[pi].name;
    g_App.players.erase(g_App.players.begin() + pi);
    g_App.selection.clear();
    g_App.anchor = Ref();
    g_App.open = Ref();
    g_App.editor.SetText("");
    g_App.editor.SetReadOnly(true);
}

static void SeedDemoWorkspace()
{
    int a = FindOrCreatePlayer("Phoenix");
    g_App.players[a].slots[0].name = "survivor1.asm";
    g_App.players[a].slots[0].code =
        "; --- PHOENIX / survivor 1 ------------------------\n"
        "; classic scanner, bombs whatever it finds\n"
        "START:\n"
        "    MOV AX, BX\n"
        "    MOV CX, 10\n"
        "    ADD AX, CX\n"
        "    CMP AX, 100\n"
        "    JE  END_LOOP\n"
        "    JMP START\n"
        "END_LOOP:\n"
        "    NOP\n";
    g_App.players[a].slots[0].present = true;
    g_App.players[a].slots[1].name = "survivor2.asm";
    g_App.players[a].slots[1].code =
        "; --- PHOENIX / survivor 2 ------------------------\n"
        "; replicator, keeps the core busy\n"
        "COPY:\n"
        "    MOV SI, [BX]\n"
        "    ADD SI, 2\n"
        "    MOV [BX], SI\n"
        "    DEC CX\n"
        "    JNZ COPY\n"
        "    INT 0x20\n";
    g_App.players[a].slots[1].present = true;

    int b = FindOrCreatePlayer("Red Comet");
    g_App.players[b].slots[0].name = "survivor1.asm";
    g_App.players[b].slots[0].code =
        "; --- RED COMET / survivor 1 ----------------------\n"
        "IMP:\n"
        "    MOV AX, 0100h\n"
        "    XOR DX, DX\n"
        "SPIN:\n"
        "    INC DX\n"
        "    CMP DX, AX\n"
        "    JB  SPIN\n"
        "    JMP IMP\n";
    g_App.players[b].slots[0].present = true;
    g_App.players[b].slots[1].name = "survivor2.asm";
    g_App.players[b].slots[1].code =
        "; --- RED COMET / survivor 2 ----------------------\n"
        "BOMB:\n"
        "    MOV DI, 2000h\n"
        "    MOV CX, 0FFh\n"
        "DROP:\n"
        "    MOV [DI], 0CCh\n"
        "    ADD DI, 4\n"
        "    LOOP DROP\n"
        "    HLT\n";
    g_App.players[b].slots[1].present = true;

    Ref r; r.player = a; r.slot = 0;
    g_App.selection.assign(1, r);
    g_App.anchor = r;
    OpenInEditor(r);
    g_App.status = "Ready - demo workspace loaded";
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

    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 8.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 6.0f;
    s.ScrollbarRounding = 8.0f;
    s.MenuItemRounding  = 5.0f;

    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.TabBarBorderSize  = 2.0f;
    s.TabBarOverlineSize = 2.0f;

    s.WindowPadding     = ImVec2(12, 10);
    s.FramePadding      = ImVec2(10, 6);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ItemInnerSpacing  = ImVec2(6, 5);
    s.CellPadding       = ImVec2(8, 5);
    s.IndentSpacing     = 18.0f;
    s.ScrollbarSize     = 11.0f;
    s.GrabMinSize       = 12.0f;
    s.SeparatorTextBorderSize = 1.0f;
    s.SeparatorTextPadding    = ImVec2(16, 4);
    s.DockingSeparatorSize    = 2.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.WindowTitleAlign  = ImVec2(0.0f, 0.5f);

    c[ImGuiCol_Text]                  = Hex(Col::Text);
    c[ImGuiCol_TextDisabled]          = Hex(Col::TextDim);
    c[ImGuiCol_WindowBg]              = Hex(Col::Bg1);
    c[ImGuiCol_ChildBg]               = Hex(Col::Bg1);
    c[ImGuiCol_PopupBg]               = Hex(Col::Bg2);
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
    c[ImGuiCol_ScrollbarGrabHovered]  = Hex(0x3A4354);
    c[ImGuiCol_ScrollbarGrabActive]   = Hex(Col::Accent, 0.75f);
    c[ImGuiCol_CheckMark]             = Hex(Col::Accent);
    c[ImGuiCol_SliderGrab]            = Hex(Col::Accent, 0.85f);
    c[ImGuiCol_SliderGrabActive]      = Hex(Col::Accent);
    c[ImGuiCol_Button]                = Hex(Col::Bg2);
    c[ImGuiCol_ButtonHovered]         = Hex(Col::Bg3);
    c[ImGuiCol_ButtonActive]          = Hex(Col::Bg4);
    c[ImGuiCol_Header]                = Hex(Col::Accent, 0.22f);
    c[ImGuiCol_HeaderHovered]         = Hex(Col::Accent, 0.16f);
    c[ImGuiCol_HeaderActive]          = Hex(Col::Accent, 0.30f);
    c[ImGuiCol_Separator]             = Hex(Col::Line);
    c[ImGuiCol_SeparatorHovered]      = Hex(Col::Accent, 0.6f);
    c[ImGuiCol_SeparatorActive]       = Hex(Col::Accent);
    c[ImGuiCol_ResizeGrip]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]     = Hex(Col::Accent, 0.4f);
    c[ImGuiCol_ResizeGripActive]      = Hex(Col::Accent, 0.7f);
    c[ImGuiCol_Tab]                   = Hex(Col::Bg1);
    c[ImGuiCol_TabHovered]            = Hex(Col::Bg3);
    c[ImGuiCol_TabSelected]           = Hex(Col::Bg2);
    c[ImGuiCol_TabSelectedOverline]   = Hex(Col::Accent);
    c[ImGuiCol_TabDimmed]             = Hex(Col::Bg1);
    c[ImGuiCol_TabDimmedSelected]     = Hex(Col::Bg2);
    c[ImGuiCol_TabDimmedSelectedOverline] = Hex(Col::Line);
    c[ImGuiCol_DockingPreview]        = Hex(Col::Accent, 0.35f);
    c[ImGuiCol_DockingEmptyBg]        = Hex(Col::Bg0);
    c[ImGuiCol_PlotLines]             = Hex(Col::Accent);
    c[ImGuiCol_PlotHistogram]         = Hex(Col::Accent2);
    c[ImGuiCol_TableHeaderBg]         = Hex(Col::Bg2);
    c[ImGuiCol_TableBorderStrong]     = Hex(Col::Line);
    c[ImGuiCol_TableBorderLight]      = Hex(Col::Line, 0.6f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = Hex(0xFFFFFF, 0.022f);
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
static int     g_PendingClosePlayer = -1;
static char    g_RenameBuf[64] = "";
static int     g_OptionsTab = 0;
static bool    g_ResetLayout = false;
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
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
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
static bool ToolButton(const char* id, int glyph, const char* tip, bool accent = false, bool enabled = true)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = 28.0f * g_Scale;
    BtnState b = BareButton(id, ImVec2(h, h));

    ImU32 bg = accent ? HexU(Col::Accent, b.held ? 1.0f : (b.hovered ? 0.95f : 0.85f))
                      : HexU(b.held ? Col::Bg4 : (b.hovered ? Col::Bg3 : Col::Bg2));
    if (!enabled) bg = HexU(Col::Bg2, 0.5f);
    dl->AddRectFilled(b.mn, b.mx, bg, 7.0f * g_Scale);

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

static void SurvivorSlotButton(int pi, int si, float size)
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

    const bool selected  = IsSelected(ref);
    const bool isOpen    = (S.open == ref);
    const bool effective = sv.present && sv.enabled && pl.enabled;

    bool clicked = b.clicked;
    if (sv.present && CheckLongPress(clicked))
        ToggleRefEnabled(ref);

    ImU32 bg, fg, border = 0;
    if (!sv.present)
    {
        // No survivor on disk for this slot: it simply shows up disabled and empty.
        bg = HexU(Col::Bg2, 0.35f);
        fg = HexU(Col::TextDim, 0.55f);
        border = HexU(Col::Line);
    }
    else if (!effective)
    {
        bg = HexU(Col::Bg2, 0.55f);
        fg = HexU(Col::TextDim);
        border = HexU(Col::Danger, 0.45f);
    }
    else if (selected)
    {
        // selected reads as the plain chip, unselected as the solid swatch
        bg = HexU(b.held ? Col::Bg4 : (b.hovered ? Col::Bg3 : Col::Bg2));
        fg = HexU(slotTint);
        border = HexU(slotTint, 0.95f);
    }
    else
    {
        bg = HexU(slotTint, b.hovered ? 1.0f : 0.82f);
        fg = HexU(Col::Bg0);
    }

    const float r = 6.0f * g_Scale;
    dl->AddRectFilled(b.mn, b.mx, bg, r);
    if (border) dl->AddRect(b.mn, b.mx, border, r, 0, 1.0f);
    if (isOpen && effective && !selected)
        dl->AddRect(b.mn, b.mx, HexU(slotTint, 0.85f), r, 0, 1.5f);

    const char label[2] = { (char)('1' + si), 0 };
    const ImVec2 tsz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2((b.mn.x + b.mx.x - tsz.x) * 0.5f, (b.mn.y + b.mx.y - tsz.y) * 0.5f), fg, label);

    if (sv.present && sv.dirty)
        dl->AddCircleFilled(ImVec2(b.mx.x - 3.5f * g_Scale, b.mn.y + 3.5f * g_Scale), 2.2f * g_Scale, HexU(Col::Warn));

    DrawLongPressHint(b.mn, b.mx);

    if (b.hovered)
    {
        if (!sv.present)
            ImGui::SetTooltip("%s %d - disabled\nThis group only loaded %d survivor%s",
                              pl.name.c_str(), si + 1, pl.LoadedCount(), pl.LoadedCount() == 1 ? "" : "s");
        else
            ImGui::SetTooltip("%s\n%s\n\nClick: open + select\nCtrl / Shift + click: multi-select\nHold: %s",
                              sv.name.c_str(),
                              sv.path.empty() ? "(unsaved)" : sv.path.c_str(),
                              sv.enabled ? "disable" : "enable");
    }

    if (clicked && sv.present)
    {
        HandleRowClick(ref, true);
        if (S.game.ready())
        {
            int w = S.game.findWarrior(pl.name, si);
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
        ImGui::TextDisabled("%s", sv.name.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem(sv.enabled ? "Disable" : "Enable")) ToggleRefEnabled(ref);
        if (ImGui::MenuItem("Save", NULL, false, !sv.path.empty())) SaveSurvivor(ref);
        if (ImGui::MenuItem("Reload from disk", NULL, false, !sv.path.empty()))
        {
            std::string text;
            if (ReadTextFile(sv.path, text))
            {
                sv.code  = text;
                sv.dirty = false;
                if (S.open == ref) { S.open = Ref(); OpenInEditor(ref); }
                S.status = "Reloaded " + sv.name;
            }
        }
        if (ImGui::MenuItem("Clear slot"))
        {
            const bool wasOpen = (S.open == ref);
            sv = Survivor();
            if (wasOpen)
            {
                S.open = Ref();
                S.editor.SetText("");
                S.editor.SetReadOnly(true);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

static void DrawPlayerRow(int pi, float rowH)
{
    AppState& S = g_App;
    Player& pl = S.players[pi];
    Ref ref; ref.player = pi; ref.slot = -1;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float  avail    = ImGui::GetContentRegionAvail().x;
    const float  slotSize = 22.0f * g_Scale;
    const float  gap      = 5.0f * g_Scale;

    ImGui::PushID(pi);

    const bool selected = IsSelected(ref);
    bool clicked = ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowOverlap, ImVec2(avail, rowH));
    const ImVec2 rowMin = ImGui::GetItemRectMin();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    const bool rowHovered = ImGui::IsItemHovered();

    if (CheckLongPress(clicked))
        ToggleRefEnabled(ref);
    DrawLongPressHint(rowMin, rowMax);

    // left tint bar
    dl->AddRectFilled(ImVec2(rowMin.x, rowMin.y + 4 * g_Scale),
                      ImVec2(rowMin.x + 3 * g_Scale, rowMax.y - 4 * g_Scale),
                      HexU(pl.tint, pl.enabled ? 1.0f : 0.30f), 1.5f * g_Scale);

    // name (clipped so it never runs under the slot buttons)
    const float  textX    = rowMin.x + 12 * g_Scale;
    const ImVec2 nsz      = ImGui::CalcTextSize(pl.name.c_str());
    const float  textY    = rowMin.y + (rowH - nsz.y) * 0.5f;
    const ImU32  nameCol  = pl.enabled ? HexU(Col::Text) : HexU(Col::TextDim);
    const float  nameClip = rowMax.x - slotSize * 2 - gap * 3;
    dl->PushClipRect(ImVec2(rowMin.x, rowMin.y), ImVec2(nameClip, rowMax.y), true);
    dl->AddText(ImVec2(textX, textY), nameCol, pl.name.c_str());
    dl->PopClipRect();

    if (rowHovered && !ImGui::IsAnyItemActive())
        ImGui::SetTooltip("%s - %d/%d survivors loaded\n\nClick: select   Ctrl / Shift + click: multi-select\nHold: %s the whole player",
                          pl.name.c_str(), pl.LoadedCount(), kSlotCount, pl.enabled ? "disable" : "enable");

    if (ImGui::BeginPopupContextItem("##rowctx"))
    {
        // Rename in place: the buffer is seeded the first frame the popup opens
        // so typing is never fighting the model.
        if (ImGui::IsWindowAppearing())
            snprintf(g_RenameBuf, sizeof(g_RenameBuf), "%s", pl.name.c_str());
        ImGui::SetNextItemWidth(180 * g_Scale);
        if (ImGui::InputText("##rename", g_RenameBuf, sizeof(g_RenameBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            if (g_RenameBuf[0]) { pl.name = g_RenameBuf; S.status = "Renamed to " + pl.name; }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere(-1);

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
        if (ImGui::MenuItem(pl.enabled ? "Disable player" : "Enable player")) ToggleRefEnabled(ref);
        if (ImGui::MenuItem("Close player")) g_PendingClosePlayer = pi;
        ImGui::EndPopup();
    }

    // the two survivor buttons, submitted after the row so they win hover/click
    float bx = rowMax.x - gap - slotSize * 2 - gap;
    const float by = rowMin.y + (rowH - slotSize) * 0.5f;
    for (int sl = 0; sl < kSlotCount; sl++)
    {
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        SurvivorSlotButton(pi, sl, slotSize);
        bx += slotSize + gap;
    }

    ImGui::PopID();

    if (clicked)
        HandleRowClick(ref, true);

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowMax.y + 3 * g_Scale));
}

static void DrawWorkspacePanel(float width, float height)
{
    AppState& S = g_App;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), HexU(Col::Bg1));

    // --- header ------------------------------------------------------------
    const float headerH = 34.0f * g_Scale;
    {
        const char* title = "GROUPS";
        // the caret takes the left edge, so the label starts after it
        const float caretW = 22.0f * g_Scale;
        const ImVec2 tsz = ImGui::CalcTextSize(title);
        if (!S.groupsCollapsed)
            dl->AddText(ImVec2(origin.x + caretW + 10 * g_Scale, origin.y + (headerH - tsz.y) * 0.5f),
                        HexU(Col::TextDim), title);

        const float bs = 22.0f * g_Scale;
        BtnState b;
        // Collapsed there is only room for the caret, so nothing else is drawn:
        // the actions would sit on top of it.
        if (!S.groupsCollapsed)
        {
            float bx = origin.x + width - bs - 10 * g_Scale;
            ImGui::SetCursorScreenPos(ImVec2(bx, origin.y + (headerH - bs) * 0.5f));
            b = BareButton("##wsload", ImVec2(bs, bs));
            if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3), 5 * g_Scale);
            DrawGlyphFolder(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 5 * g_Scale, HexU(Col::TextDim));
            if (b.hovered) ImGui::SetTooltip("Open folder (Ctrl+O)");
            if (b.clicked) OpenWorkspaceFolder();

            bx -= bs + 4 * g_Scale;
            ImGui::SetCursorScreenPos(ImVec2(bx, origin.y + (headerH - bs) * 0.5f));
            b = BareButton("##wsopensv", ImVec2(bs, bs));
            if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3), 5 * g_Scale);
            DrawGlyphSurvivor(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 5 * g_Scale, HexU(Col::TextDim));
            if (b.hovered) ImGui::SetTooltip("Open survivor (Ctrl+B)\nOne or more files, straight into the workspace");
            if (b.clicked) ImportLooseFiles();

            bx -= bs + 4 * g_Scale;
            ImGui::SetCursorScreenPos(ImVec2(bx, origin.y + (headerH - bs) * 0.5f));
            b = BareButton("##wsnew", ImVec2(bs, bs));
            if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3), 5 * g_Scale);
            DrawGlyphPlus(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 5 * g_Scale, HexU(Col::TextDim));
            if (b.hovered) ImGui::SetTooltip("New player (Ctrl+N)");
            if (b.clicked) NewPlayer();
        }

        // collapse: names go, leaving one line of survivor chips per group
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 6 * g_Scale, origin.y + (headerH - bs) * 0.5f));
        b = BareButton("##wscollapse", ImVec2(bs, bs));
        if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3), 5 * g_Scale);
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

    if (S.players.empty())
    {
        ImGui::Dummy(ImVec2(0, 10 * g_Scale));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
        ImGui::TextWrapped("No groups loaded.");
        ImGui::Dummy(ImVec2(0, 4 * g_Scale));
        ImGui::TextWrapped("Open a folder holding assembly / survivors / zombies. Files whose names "
                           "differ only by a trailing number pair into one group "
                           "(survivor1 + survivor2).");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 12 * g_Scale));
        if (ImGui::Button("Open folder...", ImVec2(-FLT_MIN, 0)))
            OpenWorkspaceFolder();
        if (ImGui::Button("New player", ImVec2(-FLT_MIN, 0)))
            NewPlayer();
    }
    else
    {
        const float rowH = 28.0f * g_Scale;
        if (S.groupsCollapsed)
        {
            // one line per group, its two survivors side by side; the zombies
            // follow under a rule of their own
            const float sz = kCollapsedChip * g_Scale, gap = 4.0f * g_Scale;
            const ImVec2 base = ImGui::GetCursorScreenPos();
            float y = 0.0f;
            for (int p = 0; p < (int)S.players.size(); p++)
            {
                ImGui::PushID(p);
                for (int sl = 0; sl < kSlotCount; sl++)
                {
                    ImGui::SetCursorScreenPos(ImVec2(base.x + sl * (sz + gap), base.y + y));
                    SurvivorSlotButton(p, sl, sz);
                }
                ImGui::PopID();
                y += sz + gap;
            }
            if (!S.zombies.empty())
            {
                const float lineY = base.y + y + gap;
                dl->AddLine(ImVec2(base.x, lineY), ImVec2(base.x + sz * 2 + gap, lineY),
                            HexU(Col::Line));
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
            for (int p = 0; p < (int)S.players.size(); p++)
                DrawPlayerRow(p, rowH);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));   // close the manual cursor placement
        }
    }

    if (!S.groupsCollapsed) DrawZombieSection();

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
                                                    : 96 * g_Scale);
                if (ix > x) dl->AddText(ImVec2(ix, ty), HexU(nerr ? Col::Danger : Col::TextDim), info);
            }

            if (!OwnerEnabledOf(S.open) || !sv->enabled)
            {
                const char* tag = "DISABLED";
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
            PushEditorMarkers(*open);
        }
    }
    ImGui::End();
}

// Colour for a session group index: match by name back to a workspace player so
// renames / custom tints carry through; zombies (gi < 0) get a fixed grey-green.
static unsigned GroupColor(int gi)
{
    if (gi < 0 || gi >= g_App.game.groupCount()) return 0x8A9BA8;
    const std::string& gn = g_App.game.groupName(gi);
    for (const Player& pl : g_App.players)
        if (pl.name == gn) return pl.tint;
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

static ImU32 WarriorColor(int wi, float alpha)
{
    // Zombies belong to no scored group, so they carry no group colour -- their
    // shade comes from the row that owns them in the groups bar.
    if (g_App.game.warriorZombie(wi))
    {
        const std::string& nm = g_App.game.replay().warriorName[(size_t)wi];
        for (const Zombie& z : g_App.zombies)
            if (z.name == nm) return ImGui::ColorConvertFloat4ToU32(Hex(z.tint, alpha));
        return ImGui::ColorConvertFloat4ToU32(Hex(Col::Warn, alpha));
    }

    const int gi = g_App.game.warriorGroup(wi);
    const int sl = g_App.game.warriorSlot(wi);
    unsigned base = GroupColor(gi);
    if (gi >= 0)
    {
        const std::string& gn = g_App.game.groupName(gi);
        for (const Player& pl : g_App.players)
            if (pl.name == gn && pl.twoTone) { base = (sl == 1) ? pl.tint2 : pl.tint; return ImGui::ColorConvertFloat4ToU32(Hex(base, alpha)); }
    }
    return ImGui::ColorConvertFloat4ToU32(Hex(SlotTint(base, sl), alpha));
}


// -----------------------------------------------------------------------------
// Edit tool
//
// With it on, double-clicking a byte in the arena, the stack or the shared page
// -- or a register in the survivor panel -- asks for a new value. The change is
// kept as a Poke against the cycle it was made at; the war is then played out
// again from the seed with that poke applied, so everything after the edit
// follows from it. The re-record is deferred until the next forward move, so a
// run of edits costs one recomputation rather than one each.
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
};
static EditTarget g_Edit;

static void RequestByteEdit(cw::s32 linear, unsigned value, const char* where)
{
    if (!g_App.editMode || !g_App.game.ready()) return;
    g_Edit = EditTarget();
    g_Edit.open = true;
    g_Edit.linear = linear;
    g_Edit.cur = value & 0xFF;
    snprintf(g_Edit.what, sizeof(g_Edit.what), "%s byte %05X", where, (unsigned)linear);
    snprintf(g_Edit.buf, sizeof(g_Edit.buf), "%02X", value & 0xFF);
}

static void RequestRegEdit(int warrior, int reg, const char* name, unsigned value)
{
    if (!g_App.editMode || !g_App.game.ready()) return;
    g_Edit = EditTarget();
    g_Edit.open = true;
    g_Edit.isReg = true;
    g_Edit.warrior = warrior;
    g_Edit.reg = reg;
    g_Edit.cur = value & 0xFFFF;
    snprintf(g_Edit.what, sizeof(g_Edit.what), "register %s", name);
    snprintf(g_Edit.buf, sizeof(g_Edit.buf), "%04X", value & 0xFFFF);
}

static void DrawEditPopup()
{
    if (g_Edit.open && !ImGui::IsPopupOpen("Edit value")) ImGui::OpenPopup("Edit value");
    ImGui::SetNextWindowSize(ImVec2(300 * g_Scale, 0), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal("Edit value", NULL, ImGuiWindowFlags_NoResize)) return;

    ImGui::TextColored(Hex(Col::TextDim), "%s", g_Edit.what);
    ImGui::TextColored(Hex(Col::TextDim, 0.7f), "round %d", g_App.game.round());
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool entered = ImGui::InputText("##val", g_Edit.buf, sizeof(g_Edit.buf),
                                          ImGuiInputTextFlags_CharsHexadecimal |
                                          ImGuiInputTextFlags_EnterReturnsTrue |
                                          ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
    ImGui::TextWrapped("Hexadecimal. The war is played out again from the seed with this change "
                       "applied, next time you move forward.");
    ImGui::PopStyleColor();
    ImGui::Separator();

    const bool ok = entered || ImGui::Button("Apply", ImVec2(110 * g_Scale, 0));
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel", ImVec2(90 * g_Scale, 0));

    if (ok)
    {
        cw::Poke p;
        p.cycle = g_App.game.cycle();
        p.isReg = g_Edit.isReg;
        const unsigned v = (unsigned)strtoul(g_Edit.buf, NULL, 16);
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
        g_App.pokes.push_back(p);
        g_App.pokesPending = true;
        g_App.pokeRound = g_App.game.round();
        char m[128];
        snprintf(m, sizeof(m), "%s -> %s at round %d  (recomputes on the next move)",
                 g_Edit.what, g_Edit.buf, g_App.pokeRound);
        g_App.status = m;
    }
    if (ok || cancel) { g_Edit.open = false; ImGui::CloseCurrentPopup(); }
    ImGui::EndPopup();
}

#include "gui_inspector.inl"   // registers / flags / code / stack / shared panels

// Row / column offsets pinned to the edges of the arena view, so a cell can be
// placed without counting. The stride grows as you zoom out so the labels never
// crowd each other.
// The grid is inset by exactly these, so the rulers sit beside the arena rather
// than on top of it and cell 0,0 can always be scrolled clear of them.
static float ArenaRulerW() { return ImGui::CalcTextSize("0000").x + 8 * g_Scale; }
static float ArenaRulerH() { return ImGui::GetTextLineHeight() + 5 * g_Scale; }

static void ArenaRulers(const ImVec2& p0, float cell, int N, float lw, float lh)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wpos = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();

    int stride = 1;
    while (stride < N && stride * cell < 46.0f * g_Scale) stride *= 2;

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
        snprintf(t, sizeof(t), "%04X", (unsigned)(y * N));
        dl->AddText(ImVec2(wpos.x + 4 * g_Scale, sy), HexU(Col::TextDim, 0.75f), t);
    }
    dl->PopClipRect();

    dl->PushClipRect(ImVec2(wpos.x + lw, wpos.y), ImVec2(wpos.x + wsz.x, wpos.y + lh), true);
    for (int x = 0; x < N; x += stride)
    {
        const float sx = p0.x + x * cell;
        if (sx + 24 * g_Scale < wpos.x + lw || sx > wpos.x + wsz.x) continue;
        snprintf(t, sizeof(t), "+%02X", (unsigned)x);
        dl->AddText(ImVec2(sx + 2 * g_Scale, wpos.y + 2 * g_Scale), HexU(Col::TextDim, 0.75f), t);
    }
    dl->PopClipRect();

    dl->AddLine(ImVec2(wpos.x + lw, wpos.y), ImVec2(wpos.x + lw, wpos.y + wsz.y), HexU(Col::Line, 0.9f));
    dl->AddLine(ImVec2(wpos.x, wpos.y + lh), ImVec2(wpos.x + wsz.x, wpos.y + lh), HexU(Col::Line, 0.9f));
    dl->PopClipRect();
}

// Where the inspected warrior's pointers land in the arena. Every one of these
// is read through a segment the engine never lets leave the arena, so only the
// offset half picks the cell -- DS:BX, SS:SP and the rest all land in the same
// 64K sheet the grid is showing.
static void ArenaPointerRings(cw::GuiSession& g, const ImVec2& p0, float cell, int N,
                              int x0, int y0, int x1, int y1)
{
    const AppState& S = g_App;
    if (!(S.hlBase || S.hlIndex || S.hlPointer)) return;

    int wi = S.inspectWarrior;
    if (wi < 0 || wi >= g.numWarriors()) wi = g.player().actingWarrior();
    if (wi < 0 || wi >= g.numWarriors()) return;

    const cw::CpuState& st = g.player().cpu(wi);
    struct P { const char* n; int off; bool on; };
    const P pts[] = {
        { "BX", (int)(cw::u16)st.bx, S.hlBase    },
        { "BP", (int)(cw::u16)st.bp, S.hlBase    },
        { "SI", (int)(cw::u16)st.si, S.hlIndex   },
        { "DI", (int)(cw::u16)st.di, S.hlIndex   },
        { "SP", (int)(cw::u16)st.sp, S.hlPointer },
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rounding = (cell >= 9.0f) ? ImClamp(cell * 0.16f, 1.0f, 4.0f) : 0.0f;
    const bool  label    = cell >= 14.0f * g_Scale;

    for (const P& p : pts)
    {
        if (!p.on) continue;
        const int x = p.off % N, y = p.off / N;
        if (x < x0 - 1 || x > x1 || y < y0 - 1 || y > y1) continue;
        const ImVec2 a(p0.x + x * cell, p0.y + y * cell);
        const ImVec2 mn(a.x - 1.5f, a.y - 1.5f), mx(a.x + cell + 0.5f, a.y + cell + 0.5f);
        dl->AddRect(mn, mx, HexU(S.hlColor, 0.95f), rounding, 0, ImMax(1.4f, cell * 0.12f));
        if (label)
        {
            const ImVec2 ts = ImGui::CalcTextSize(p.n);
            dl->AddRectFilled(ImVec2(mn.x, mn.y - ts.y), ImVec2(mn.x + ts.x + 4 * g_Scale, mn.y),
                              HexU(S.hlColor, 0.9f), 2.0f);
            dl->AddText(ImVec2(mn.x + 2 * g_Scale, mn.y - ts.y), HexU(Col::Bg0), p.n);
        }
    }
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
    const float lw = g_App.arenaRulers ? ArenaRulerW() : 0.0f;
    const float lh = g_App.arenaRulers ? ArenaRulerH() : 0.0f;

    const ImVec2 wpos = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();
    const ImVec2 viewMin(wpos.x + lw, wpos.y + lh);           // where the grid starts
    const ImVec2 viewSz(ImMax(1.0f, wsz.x - lw), ImMax(1.0f, wsz.y - lh));

    ImGui::SetCursorScreenPos(viewMin);
    ImGui::InvisibleButton("##arenapan", viewSz,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                           ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();

    ImVec2& pan = g_App.arenaPan;                             // top-left byte on screen

    // wheel zooms about the cursor: solve for the pan that keeps that byte put
    if (hovered && io.MouseWheel != 0.0f)
    {
        const float nz = ImClamp(cell * powf(1.15f, io.MouseWheel), 1.0f, 512.0f);
        if (nz != cell)
        {
            const float bx = pan.x + (io.MousePos.x - viewMin.x) / cell;
            const float by = pan.y + (io.MousePos.y - viewMin.y) / cell;
            pan.x = bx - (io.MousePos.x - viewMin.x) / nz;
            pan.y = by - (io.MousePos.y - viewMin.y) / nz;
            g_App.arenaZoom = cell = nz;
        }
    }

    if (ImGui::IsItemActive())
    {
        pan.x -= io.MouseDelta.x / cell;
        pan.y -= io.MouseDelta.y / cell;
    }

    // Centre the arena on an axis where the whole thing fits; otherwise keep it
    // against the edges so there is never dead space to pan into.
    const float visX = viewSz.x / cell, visY = viewSz.y / cell;
    pan.x = (visX >= (float)N) ? ((float)N - visX) * 0.5f : ImClamp(pan.x, 0.0f, (float)N - visX);
    pan.y = (visY >= (float)N) ? ((float)N - visY) * 0.5f : ImClamp(pan.y, 0.0f, (float)N - visY);

    const ImVec2 p0(viewMin.x - pan.x * cell, viewMin.y - pan.y * cell);   // cell 0,0

    const int x0 = ImMax(0, (int)floorf(pan.x)),        x1 = ImMin(N, (int)ceilf(pan.x + visX) + 1);
    const int y0 = ImMax(0, (int)floorf(pan.y)),        y1 = ImMin(N, (int)ceilf(pan.y + visY) + 1);
    dl->PushClipRect(viewMin, ImVec2(viewMin.x + viewSz.x, viewMin.y + viewSz.y), true);

    const cw::u8* bytes = g.arena();

    // The two hex digits are drawn at a size derived from the cell, so they
    // appear as soon as they can possibly fit and stay centred as you zoom.
    const float hexSize = ImFloor(cell * 0.56f);
    const bool  showHex = bytes && g_App.fontMono && hexSize >= 6.0f;

    // Cells only separate once there is text to separate; below that the arena
    // reads better as a solid sheet of ownership.
    const float gap      = showHex ? ImClamp(cell * 0.10f, 0.6f, 2.5f) : 0.0f;
    const float rounding = showHex ? ImClamp(cell * 0.16f, 1.0f, 4.0f) : 0.0f;
    if (showHex) ImGui::PushFont(g_App.fontMono, hexSize);
    const float digitW = showHex ? ImGui::CalcTextSize("FF").x : 0.0f;
    const float digitH = showHex ? ImGui::GetFontSize() : 0.0f;

    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
        {
            const int off = y * N + x;
            const int wi  = g.ownerAt(off);
            ImU32 col;
            if (wi >= 0) col = WarriorColor(wi, g.warriorAlive(wi) ? 0.95f : 0.34f);
            // Untouched core: this is the colour the option changes. The sheet
            // behind the grid stays whatever the theme says.
            else         col = HexU(g_App.arenaBgCustom ? g_App.arenaBg : Col::Bg1);
            const ImVec2 a(p0.x + x * cell, p0.y + y * cell);
            const ImVec2 b(a.x + cell - gap, a.y + cell - gap);
            dl->AddRectFilled(a, b, col, rounding);
            if (showHex)
            {
                char h[3];
                static const char* kHex = "0123456789ABCDEF";
                h[0] = kHex[(bytes[off] >> 4) & 0xF];
                h[1] = kHex[bytes[off] & 0xF];
                h[2] = 0;
                const ImU32 tc = (wi >= 0) ? HexU(Col::Bg0, 0.92f) : HexU(Col::TextDim, 0.75f);
                dl->AddText(ImVec2(a.x + (cell - digitW) * 0.5f, a.y + (cell - digitH) * 0.5f), tc, h);
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
        const ImVec2 mn(a.x - 1, a.y - 1), mx(a.x + cell + 1, a.y + cell + 1 - gap);
        dl->AddRect(mn, mx, WarriorColor(wi, 1.0f), ipRound, 0, ImMax(1.5f, cell * 0.16f));
        dl->AddRect(mn, mx, HexU(ContrastInk(), 0.9f), ipRound, 0, 1.0f);
    }

    ArenaPointerRings(g, p0, cell, N, x0, y0, x1, y1);
    dl->PopClipRect();
    if (g_App.arenaRulers) ArenaRulers(p0, cell, N, lw, lh);
}

// A small square toggle used for the two binary transport switches.
static bool ToggleChip(const char* id, const char* label, bool* v, const char* tip)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float h = 28.0f * g_Scale;
    const float w = ts.x + 16 * g_Scale;
    BtnState b = BareButton(id, ImVec2(w, h));
    const bool on = *v;
    dl->AddRectFilled(b.mn, b.mx,
                      HexU(on ? Col::Accent : (b.hovered ? Col::Bg3 : Col::Bg2), on ? 0.85f : 1.0f),
                      7.0f * g_Scale);
    dl->AddText(ImVec2(b.mn.x + 8 * g_Scale, (b.mn.y + b.mx.y - ts.y) * 0.5f),
                HexU(on ? Col::Bg0 : Col::TextDim), label);
    if (b.hovered && tip) ImGui::SetTooltip("%s", tip);
    if (b.clicked) *v = !*v;
    return b.clicked;
}

#include "gui_timeline.inl"   // step-column / survivor-lane timeline

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
static void FocusArenaOnIp()
{
    AppState& S = g_App;
    if (!S.game.ready()) return;
    int wi = S.inspectWarrior;
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

        // transport: back | play/pause | forward, then continue and rewind.
        // Everything moves in whole rounds -- one pass in which every living
        // warrior takes its turn.
        if (ToolButton("##back", 7, "Back - one round (Shift+F11)", false, ready)) GameStepRound(-1);
        ImGui::SameLine(0, 4 * g_Scale);
        if (ToolButton("##dbgrun", run ? 1 : 0,
                       busy ? "Precomputing..." : (run ? "Pause (F5)" : (ready ? "Run (F5)" : "Precompute + play (F5)")),
                       true, !busy))
            GamePlayPause();
        ImGui::SameLine(0, 4 * g_Scale);
        if (ToolButton("##fwd", 8, "Forward - one round (F11)", false, ready)) GameStepRound(+1);
        ImGui::SameLine(0, 8 * g_Scale);
        if (ToolButton("##cont", 9, "Continue to the next event - death or BRK (F8)", false, ready)) GameContinue();
        ImGui::SameLine(0, 4 * g_Scale);
        if (ToolButton("##rew", 3, "Rewind to the start (Shift+F5)", false, ready))           GameReset();

        ImGui::SameLine(0, 12 * g_Scale);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (28 * g_Scale - ImGui::GetFrameHeight()) * 0.5f);
        ImGui::SetNextItemWidth(120 * g_Scale);
        ImGui::SliderFloat("##speedexp", &S.speedExp, 0.0f, 30.0f, "");
        ImGui::SameLine(0, 8 * g_Scale);
        ImGui::TextColored(Hex(Col::Accent2), "%.0f r/s", RoundsPerSec());

        // seed + recompute: the two things you reach for to run the fight again
        ImGui::SameLine(0, 12 * g_Scale);
        ImGui::SetNextItemWidth(96 * g_Scale);
        if (ImGui::InputTextWithHint("##seed", "seed", S.seedText, sizeof(S.seedText),
                                     ImGuiInputTextFlags_EnterReturnsTrue))
            GameArm();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Text or a number, resolved the way the reference does:\\n"
                              "a plain number is used as-is, anything else is hashed.\\n"
                              "-> %lld", (long long)SeedFromText(S.seedText));
        ImGui::SameLine(0, 6 * g_Scale);
        if (ImGui::Button(busy ? "..." : "Recompute") && !busy) GameArm();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play the whole war out again from this seed (Ctrl+F5)");

        ImGui::SameLine(0, 6 * g_Scale);
        if (ImGui::Button("Find IP") && ready) FocusArenaOnIp();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Zoom the arena onto the inspected survivor's next instruction");

        ImGui::SameLine(0, 10 * g_Scale);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (28 * g_Scale - ImGui::GetFrameHeight()) * 0.5f);
        ToggleChip("##dbgstop", "DBG", &S.game.stopOnEvents, "Stop when a death or breakpoint is reached");
        ImGui::SameLine(0, 4 * g_Scale);
        ToggleChip("##dbgrev", "REV", &S.game.reverse, "Run and continue backwards through the recording");
        ImGui::SameLine(0, 4 * g_Scale);
        ToggleChip("##dbgedit", "EDIT", &S.editMode,
                   "Double-click a byte or a register to change it.\n"
                   "The war is played out again with the change on the next move.");

        ImGui::SameLine(0, 14 * g_Scale);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (28 * g_Scale - ImGui::GetTextLineHeight()) * 0.5f);
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

#include "gui_gpulane.inl"   // the Vulkan engine, run as a child process
#include "gui_javalane.inl"  // the CGX Java engine, run the same way
#include "gui_net.inl"       // shard files and the host / join wire
#include "gui_compete.inl"   // tournament runner + score board

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
    char exe[MAX_PATH] = "";
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    std::string p = exe;
    const size_t slash = p.find_last_of("\\/");
    return (slash == std::string::npos ? std::string() : p.substr(0, slash + 1)) + "cwest.ini";
}

// One list, walked twice: once to write and once to read, so a new setting
// cannot be saved but not loaded.
#define CWEST_SETTINGS(B, I, F, U, S)                                          \
    B(inspectorLeft)      B(groupsCollapsed)   B(showInspector)                \
    B(showTimeline)       B(showEditor)        B(showArena)      B(showBoard)  \
    B(arenaRulers)        B(arenaBgCustom)     B(advanceSeed)                  \
    B(asmUpperCase)       B(showAsmGutter)     B(dblClickDisasm)               \
    B(followSS)           B(followES)          B(followCS)                     \
    B(hlBase)             B(hlIndex)           B(hlPointer)                    \
    B(breakStartsWith)    B(breakOnTimeline)                                   \
    B(folderAssemble)     B(folderDisassemble)                                 \
    I(themeIndex)         I(folderSortMode)    I(zombieSpeed)   I(maxRounds)   \
    B(compBarsUp)         B(compBarsFit)                                       \
    I(compGames)          I(compThreads)       I(compByGroup)                  \
    I(compCombo)          I(compPairSeed)      I(compBackend)                  \
    I(compMode)           I(compNetPort)       I(compShards)                   \
    F(inspectorWidth)     F(workspaceWidth)    F(fontUISize)                   \
    F(fontMonoSize)       F(fontEditorSize)    F(speedExp)      F(arenaZoom)   \
    U(arenaBg)            U(hlColor)                                           \
    S(seedText)           S(breakPatterns)     S(compNetHost)   S(compNetName)

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
    // the palette, so a hand-edited theme survives
    for (int i = 0; i < kPalCount; i++) f << "pal" << i << "=" << *PalSlot(i) << "\n";
}

static void LoadSettings()
{
    std::ifstream f(SettingsPath().c_str());
    if (!f) return;
    AppState& S = g_App;
    std::string line;
    while (std::getline(f, line))
    {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);
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
        if (k == "lastFolder") { S.lastFolder = v; continue; }
        if (k.compare(0, 3, "pal") == 0)
        {
            const int i = atoi(k.c_str() + 3);
            if (i >= 0 && i < kPalCount) *PalSlot(i) = (unsigned)strtoul(v.c_str(), NULL, 10);
        }
    }
}

#include "gui_options.inl"   // options window, folder flow, zombie section

static void HandleShortcuts()
{
    AppState& S = g_App;
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Q))              ToggleSelectionEnabled();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))              NewPlayer();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))              OpenWorkspaceFolder();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_B))              ImportLooseFiles();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Comma))          g_App.showOptions = !g_App.showOptions;

    // Delete closes the selected warriors -- but never while a text field or the
    // source editor has the keyboard.
    if (!io.WantTextInput && !g_App.selection.empty() &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        DeleteSelection();

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
        if (sv && sv->present)
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
            else if (!g_ReframeRedo.empty() &&
                     ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y))
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

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !S.selection.empty() && !ImGui::IsAnyItemActive())
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
static ImFont* TryLoadFont(const char* const* candidates, int count, float sizePx)
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;
    cfg.Flags |= ImFontFlags_NoLoadError;

    static const char* roots[] = { "", "fonts/", "engine/fonts/" };
    for (int i = 0; i < count; i++)
        for (const char* root : roots)
        {
            const std::string path = std::string(root) + candidates[i];
            if (!FileExists(path)) continue;
            if (ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), sizePx, &cfg))
                return f;
        }
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

    S.fontUI = TryLoadFont(uiFonts, IM_ARRAYSIZE(uiFonts), S.fontUISize * g_Scale);
    if (!S.fontUI) S.fontUI = io.Fonts->AddFontDefault();
    S.fontMono = TryLoadFont(monoFonts, IM_ARRAYSIZE(monoFonts), S.fontMonoSize * g_Scale);
    if (!S.fontMono) S.fontMono = S.fontUI;

    io.FontDefault = S.fontUI;
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

// Dropping files: source files (.asm/.s/.txt) become editable survivors, files
// with no such extension are treated as assembled machine code.
static void OnDropFiles(GLFWwindow*, int count, const char** paths)
{
    std::vector<std::string> src, bin;
    for (int i = 0; i < count; i++)
    {
        std::string p = paths[i];
        std::string ext = ToLower(p.size() >= 4 ? p.substr(p.find_last_of('.') == std::string::npos ? p.size() : p.find_last_of('.')) : "");
        if (ext == ".asm" || ext == ".s" || ext == ".txt") src.push_back(p);
        else                                               bin.push_back(p);
    }
    if (!src.empty()) LoadSurvivorFiles(src);
    if (!bin.empty()) LoadBinarySurvivorFiles(bin);
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
int main(int argc, char** argv)
{
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
    io.IniFilename = "corewars_layout.ini";

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    LoadSettings();
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
    //   --folder <dir>        open a workspace folder
    //   --serve [port]        host: listen, and Run the tournament straight away
    //   --join host[:port]    play a share of the tournament at host
    //   --games <n>           battles for the run (0 = until Stop)
    //   --threads <n>         worker threads for this machine (0 = every core)
    //   --engine cpu|gpu|both|java which lanes this machine plays with
    //   --mode local|offline|server|join
    //   --run                 press Run once the field is loaded
    bool autoRun = false;
    {
        std::vector<const char*> a;
        for (int i = 1; i < argc; i++)
        {
            const std::string arg = argv[i];
            const bool more = (i + 1 < argc);
            if (arg == "--folder" && more)      { LoadWorkspaceFolder(argv[++i]); continue; }
            if (arg == "--games" && more)       { g_App.compGames = atoi(argv[++i]); continue; }
            if (arg == "--threads" && more)     { g_App.compThreads = atoi(argv[++i]); continue; }
            if (arg == "--mode" && more)
            {
                const std::string m = ToLower(argv[++i]);
                g_App.compMode = (m == "offline") ? MODE_OFFLINE : (m == "server") ? MODE_SERVER
                               : (m == "join") ? MODE_JOIN : MODE_LOCAL;
                continue;
            }
            if (arg == "--run")                 { autoRun = true; continue; }
            if (arg == "--engine" && more)
            {
                const std::string e = ToLower(argv[++i]);
                g_App.compBackend = (e == "gpu") ? COMP_GPU : (e == "both") ? COMP_BOTH
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
                std::string hp = argv[++i];
                const size_t colon = hp.find(':');
                if (colon != std::string::npos) { g_App.compNetPort = atoi(hp.c_str() + colon + 1); hp = hp.substr(0, colon); }
                snprintf(g_App.compNetHost, sizeof(g_App.compNetHost), "%s", hp.c_str());
                g_App.compMode = MODE_JOIN;
                g_App.showBoard = true;
                g_CompRaise = true;
                std::string why;
                if (!NetClientConnect(g_App.compNetHost, g_App.compNetPort, why)) g_NetClient.note = why;
                continue;
            }
            a.push_back(argv[i]);
        }
        if (!a.empty()) OnDropFiles(window, (int)a.size(), a.data());
    }
    if (g_App.players.empty() && g_App.compMode != MODE_JOIN)
        SeedDemoWorkspace();
    if (autoRun) { g_App.showBoard = true; g_CompRaise = true; CompetitionStart(); }

    bool layoutBuilt = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            Sleep(10);
            continue;
        }

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
        DrawWorkspacePanel(wsW, bodyH);

        // splitter
        {
            ImGui::SetCursorScreenPos(ImVec2(rootMin.x + wsW, bodyTop));
            BtnState sp = BareButton("##splitter", ImVec2(splitW, bodyH));
            if (sp.held) S.workspaceWidth += ImGui::GetIO().MouseDelta.x;
            if (sp.hovered || sp.held) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            ImGui::GetWindowDrawList()->AddRectFilled(
                sp.mn, sp.mx, HexU((sp.hovered || sp.held) ? Col::Accent : Col::Line, (sp.hovered || sp.held) ? 0.85f : 1.0f));
        }

        // dockspace: one node, every panel tabbed into it
        const ImGuiID dockId = ImGui::GetID("CoreWarsDockSpace");
        const ImVec2 dockPos(rootMin.x + S.workspaceWidth + splitW, bodyTop);
        const ImVec2 dockSize(ImMax(60.0f, rootW - S.workspaceWidth - splitW), bodyH);
        ImGui::SetCursorScreenPos(dockPos);
        ImGui::DockSpace(dockId, dockSize, ImGuiDockNodeFlags_NoWindowMenuButton);

        if (!layoutBuilt || g_ResetLayout)
        {
            if (g_ResetLayout || ImGui::DockBuilderGetNode(dockId) == NULL)
                BuildDockLayout(dockId, dockSize);
            layoutBuilt = true;
            g_ResetLayout = false;
        }

        ImGui::SetCursorScreenPos(ImVec2(rootMin.x, bodyTop + bodyH));
        DrawStatusBar(statusH);

        ImGui::End(); // CoreWarsRoot

        // Docked panels are submitted after the host window is closed.
        if (S.showEditor) DrawEditorPanel();
        if (S.showArena)  DrawArenaPanel();
        if (S.showBoard)  DrawLeaderboardPanel();
        DrawPoppedInspectors(S.game);
        if (S.showDemo)   ImGui::ShowDemoWindow(&S.showDemo);
        DrawAboutPopup();
    DrawEditPopup();
    DrawOptionsWindow();

        if (g_PendingClosePlayer >= 0)
        {
            ClosePlayer(g_PendingClosePlayer);
            g_PendingClosePlayer = -1;
        }
        HandleShortcuts();

        {   // --- TEMP DIAGNOSTIC -------------------------------------------
            static std::ofstream dbg("dock_dbg.txt");
            static std::string prev;
            std::ostringstream ss;
            const char* names[3] = { "Editor", "Debug", "Competition" };
            ss << "dockId=" << (unsigned)dockId;
            for (int i = 0; i < 3; i++)
            {
                ImGuiWindow* w = ImGui::FindWindowByName(names[i]);
                ss << " | " << names[i] << ": ";
                if (!w) { ss << "none"; continue; }
                ss << "DockId=" << (unsigned)w->DockId
                   << " active=" << (w->DockIsActive ? 1 : 0)
                   << " node=" << (w->DockNode ? 1 : 0)
                   << " appearing=" << (w->Appearing ? 1 : 0)
                   << " hidden=" << (w->Hidden ? 1 : 0);
            }
            std::string line = ss.str();
            if (line != prev)
            {
                dbg << "frame " << ImGui::GetFrameCount() << "  " << line << std::endl;
                dbg.flush();
                prev = line;
            }
        }

        // --- render ----------------------------------------------------------
        ImGui::Render();
        g_NoDragActive.swap(g_NoDragBuild);

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        const ImVec4 clear = Hex(Col::Bg0);
        glClearColor(clear.x, clear.y, clear.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    FlushEditorToModel();
    SaveSettings();
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
