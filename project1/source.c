#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1

#include <stdio.h>
#include <conio.h>
#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define H 14
#define W 33

#define UI_W 160
#define UI_H 45

#define M_X 41
#define M_Y 4
#define M_W 70
#define M_H 18

#define MSG_X 41
#define MSG_Y (M_H + M_Y + 2)
#define MSG_W 70
#define MSG_H 7

#define STAT_X 3
#define STAT_Y 3
#define STAT_W 25
#define STAT_H 15

#define DECK_BOX_X 3
#define DECK_BOX_Y 20
#define DECK_BOX_W 25
#define DECK_BOX_H 16

#define CARD_UI_X (UI_W - 30)
#define CARD_UI_Y 4

#define CODEX_COLS 6
#define CODEX_ROWS 3
#define CODEX_SLOTS (CODEX_COLS * CODEX_ROWS) // 18

#define SLOT_W 22
#define SLOT_H 8

#define CODEX_X 6
#define CODEX_Y 4
#define SLOT_GAP_X 3
#define SLOT_GAP_Y 1

#define FLOORS 5
#define SKILLS_PER_FLOOR 18
#define MAX_SKILLS (FLOORS * SKILLS_PER_FLOOR) // 54

#define MAX_EQUIP 6
#define ENERGY_MAX 3

#define PASSIVE_GRIT_HEART   6
#define PASSIVE_FINAL_PREP   7


#define STORE_BOX_X 3
#define STORE_BOX_Y 16
#define STORE_BOX_W 25
#define STORE_BOX_H 16

// printf/putchar를 백버퍼 출력으로 대체
#define printf  DB_printf
#define putchar DB_putchar
#define MAX_PASSIVE 3
#define VIS_R 6

int moveCountSinceLastBattle = 0;

// ---------------------------
// 더블버퍼(CHAR_INFO) 기반 렌더
// ---------------------------
static HANDLE gCon = NULL;

static int gUIW = UI_W;
static int gUIH = UI_H;

static int gWinW = UI_W;
static int gWinH = UI_H;

static int gOffX = 0;
static int gOffY = 0;

static int gBufW = UI_W;
static int gBufH = UI_H;

static CHAR_INFO* gBack = NULL; // 메모리 백버퍼

static int gCurX = 0, gCurY = 0; // 버퍼 커서
static WORD gAttr = 0;
static void DB_ClearAll(void)
{
    if (!gBack) return;
    for (int i = 0; i < gBufW * gBufH; i++) {
        gBack[i].Char.AsciiChar = ' ';
        gBack[i].Attributes = gAttr;
    }
    gCurX = 0;
    gCurY = 0;
}
static void EnsureConsoleBufferAtLeast(int w, int h)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(gCon, &csbi);

    int curBW = csbi.dwSize.X;
    int curBH = csbi.dwSize.Y;

    if (curBW >= w && curBH >= h) return;

    COORD newSize;
    newSize.X = (SHORT)((curBW < w) ? w : curBW);
    newSize.Y = (SHORT)((curBH < h) ? h : curBH);

    // 버퍼를 먼저 키우고
    SetConsoleScreenBufferSize(gCon, newSize);
}

static void DB_UpdateWindowInfo(void)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(gCon, &csbi);
    gAttr = csbi.wAttributes;

    gWinW = (csbi.srWindow.Right - csbi.srWindow.Left + 1);
    gWinH = (csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    // 버퍼는 윈도우 크기 이상으로 확보(스크롤바/잘림 방지)
    int needW = (gWinW > gUIW) ? gWinW : gUIW;
    int needH = (gWinH > gUIH) ? gWinH : gUIH;
    EnsureConsoleBufferAtLeast(needW, needH);
    if (needW != gBufW || needH != gBufH || gBack == NULL) {
        gBufW = needW;
        gBufH = needH;
        if (gBack) free(gBack);
        gBack = (CHAR_INFO*)malloc(sizeof(CHAR_INFO) * gBufW * gBufH);
    }

    // 중앙 오프셋 계산
    gOffX = (gWinW > gUIW) ? (gWinW - gUIW) / 2 : 0;
    gOffY = (gWinH > gUIH) ? (gWinH - gUIH) / 2 : 0;
    if (gOffX < 0) gOffX = 0;
    if (gOffY < 0) gOffY = 0;
}

void DB_Init(void)
{
    gCon = GetStdHandle(STD_OUTPUT_HANDLE);

    // 커서 숨김(윈도우 핸들 기반)
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(gCon, &info);
    info.bVisible = FALSE;
    SetConsoleCursorInfo(gCon, &info);

    DB_UpdateWindowInfo();
}

// 프레임 시작(원문에 호출이 있어서 추가)
void DB_BeginFrame(void)
{
    DB_UpdateWindowInfo();
    DB_ClearAll();
}

// 백버퍼 특정 좌표에 문자 찍기(화면 밖이면 무시)
static void DB_PutCharAt(int x, int y, char c)
{
    if (x < 0 || y < 0 || x >= gBufW || y >= gBufH) return;
    int idx = y * gBufW + x;
    gBack[idx].Char.AsciiChar = c;
    gBack[idx].Attributes = gAttr;
}

// 문자열 쓰기(자동 줄바꿈 최소 처리)
static void DB_WriteRaw(const char* s)
{
    if (!s) return;
    while (*s) {
        char c = *s++;

        if (c == '\n') {
            gCurX = 0;
            gCurY++;
            continue;
        }
        if (c == '\r') {
            gCurX = 0;
            continue;
        }

        int tx = gCurX;
        int ty = gCurY;

        DB_PutCharAt(tx, ty, c);

        gCurX++;
        if (gCurX >= gBufW) {
            gCurX = 0;
            gCurY++;
        }
        if (gCurY >= gBufH) {
            gCurY = gBufH - 1;
        }
    }
}

// 백버퍼를 콘솔에 한 번에 출력
void DB_Present(void)
{
    if (!gBack) return;

    // 창 크기 정보 갱신(중앙 오프셋 포함)
    DB_UpdateWindowInfo();

    SMALL_RECT rect;
    rect.Left = 0;
    rect.Top = 0;
    rect.Right = (SHORT)((gWinW - 1 < gBufW - 1) ? (gWinW - 1) : (gBufW - 1));
    rect.Bottom = (SHORT)((gWinH - 1 < gBufH - 1) ? (gWinH - 1) : (gBufH - 1));


    COORD bufSize = { (SHORT)gBufW, (SHORT)gBufH };
    COORD bufPos = { 0, 0 };

    WriteConsoleOutputA(gCon, gBack, bufSize, bufPos, &rect);
}

int DB_putchar(int c)
{
    char s[2];
    s[0] = (char)c;
    s[1] = '\0';
    DB_WriteRaw(s);
    return c;
}

int DB_printf(const char* fmt, ...)
{
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    DB_WriteRaw(tmp);
    return (int)strlen(tmp);
}

static void DB_PrimeBeforeTitle(void)
{
    // 창/버퍼/백버퍼 크기 계산 및 확보
    DB_UpdateWindowInfo();

    // 백버퍼 비우고 한번 출력해서 "깨짐 상태"를 초기화
    DB_ClearAll();
    DB_Present();
}

void gotoxy(int x, int y)
{
    // UI좌표 -> 백버퍼 좌표 (중앙 오프셋 적용)
    gCurX = x + gOffX;
    gCurY = y + gOffY;
}

// 입력 대기(딜레이 대신)
void wait_any_key()
{
    DB_Present();
   (void)_getch();
}

// --------------------
// 방향/플레이어/적
// --------------------
typedef enum { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 } Dir;
typedef struct { int x, y; } Pos;

static const Pos JOURNAL_POS[FLOORS][3] = {
    // floor 0의 일지 3개 위치 (x,y)
    { {1,4}, {14,4}, {1,10} },
    // floor 1
    { {9,3}, {17,7}, {7,12} },
    // floor 2
    { {21,8}, {11,10}, {21,10} },
    // floor 3
    { {16,6}, {0,0}, {0,0} },
};

typedef enum {
    POT_HEAL25 = 0,
    POT_HEAL50,
    POT_GUARD10,
    POT_GUARD30,
    POT_COUNT
} PotionType;

typedef struct {
    int x, y;
    int HP, maxHP;
    char name[64];
    Dir dir;
    int level;
    int exp;
    int expToNext;

    // 장착 슬롯 6개
    int equip[MAX_EQUIP];
    int equipCount;

    // 체크포인트
    int cpX, cpY;
    int hasCheckpoint;

    // 현재 층(0..2)
    int floor;

    int poison;
    int burn;
    int vuln;
    int weak;

    int gold;
    int altarGuardBonus;

    unsigned char journalMask[FLOORS];

    int passiveSlots[MAX_PASSIVE];   // -1이면 비어있음
    int passiveCount;

    int potions[POT_COUNT];

    unsigned char seen[FLOORS][H][W];   // 한 번이라도 본 타일
    unsigned char visible[H][W];
    int boss4Dead;
    int gritUsed;
} Player;


int GetJournalIndex(int floor, int x, int y)
{
    for (int i = 0; i < 3; i++) {
        if (JOURNAL_POS[floor][i].x == x && JOURNAL_POS[floor][i].y == y)
            return i;
    }
    return -1;
}

typedef enum { EFF_NONE = 0, EFF_POISON, EFF_BURN, EFF_VULN, EFF_WEAK } EffectType;

typedef enum { ENC_NORMAL = 0, ENC_ELITE = 1, ENC_BOSS = 2 } EncounterType;

typedef enum {
    EN_NORMAL = 0,
    EN_ELITE_GUARD = 10,
    EN_ELITE_HUNTER = 11,
    EN_ELITE_KNIGHT = 12,
    EN_BOSS_GATEKEEPER = 20,   // 1층 
    EN_BOSS_EXECUTIONER = 21,  // 2층 
    EN_BOSS_PLAGUE_CORE = 22,  // 3층 
    EN_BOSS_FINAL = 23
} EnemyKind;

typedef struct {
    char name[20];
    int hp;
    int maxHp;
    int atk;
    int intent;
    int expReward;
    int poison;
    int burn;
    int vuln;
    int weak;

    int kind;
   
    
} Enemy;

// 스킬(도감 DB 54개)
typedef struct {
    int id;
    char name[32];
    int cost;
    int dmg, block, heal;
    int unlocked;
    char dec[80];
    EffectType eff;
    int effVal;
} Skill;

Skill codexSkills[MAX_SKILLS];
int codexCount = MAX_SKILLS;
// 상인
typedef struct {
    int passiveId;
    int price;
} PassiveOffer;
static int InBounds(int x, int y) {
    return (x >= 0 && x < W && y >= 0 && y < H);
}

static const PassiveOffer PASSIVE_BY_FLOOR[FLOORS][2] = {
    { {0, 50}, {1, 55} }, // 1층: 최대체력, 전투방어
    { {2, 85}, {4, 70} }, // 2층: 에너지, 상태이상
    { {5, 90}, {3, 75} }, // 3층: 0비용2회, 처치골드
    { {6,120},  {7,120}  }, // 4층: 집념/ 마지막 준비
    { {6,120},  {7,120}  },
};


void UpdateFOV(Player* p, char map[H][W + 1])
{
    // visible 초기화
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p->visible[y][x] = 0;

    // 간단 원형 시야
    for (int dy = -VIS_R; dy <= VIS_R; dy++) {
        for (int dx = -VIS_R; dx <= VIS_R; dx++) {
            int nx = p->x + dx;
            int ny = p->y + dy;
            if (!InBounds(nx, ny)) continue;

            // 원형 느낌 
            if (dx * dx + dy * dy > VIS_R * VIS_R) continue;

            p->visible[ny][nx] = 1;
            p->seen[p->floor][ny][nx] = 1;
        }
    }
}

// dir 기준으로 forward(perp) 벡터 계산
static void DirVec(Dir d, int* fx, int* fy, int* px, int* py)
{
    // forward
    *fx = 0; *fy = 0;
    if (d == NORTH) *fy = -1;
    else if (d == SOUTH) *fy = 1;
    else if (d == WEST)  *fx = -1;
    else if (d == EAST)  *fx = 1;

    // perpendicular (오른쪽 방향)
    *px = -*fy;
    *py = *fx;
}

// 이동할 때마다: "전방 2칸"을 폭 3(좌/중/우)로 밝힘
void RevealForward2(Player* p, char map[H][W + 1])
{
    int fx, fy, px, py;
    DirVec(p->dir, &fx, &fy, &px, &py);

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = p->x + dx;
            int ny = p->y + dy;
            if (InBounds(nx, ny)) p->seen[p->floor][ny][nx] = 1;
        }
    }
    // 현재 칸도 본 것으로
    if (InBounds(p->x, p->y))
        p->seen[p->floor][p->y][p->x] = 1;

    int sideRange = 2;   // 좌우 1칸(폭 3)
    int forwardRange = 2; // 전방 2칸

    for (int step = 1; step <= forwardRange; step++) {
        for (int s = -sideRange; s <= sideRange; s++) {
            int nx = p->x + fx * step + px * s;
            int ny = p->y + fy * step + py * s;
            if (!InBounds(nx, ny)) continue;

            p->seen[p->floor][ny][nx] = 1;

           
           
        }
      
      
    }
}


const char* GetPassiveName(int id)
{
    switch (id) {
    case 0: return "체력의 유물";
    case 1: return "수호의 유물";
    case 2: return "심장의 유물";
    case 3: return "연금의 유물";
    case 4: return "독불의 유물";
    case 5: return "경량화 유물";
    case PASSIVE_GRIT_HEART: return "집념의 심장";
    case PASSIVE_FINAL_PREP: return "마지막 준비";
    default: return "알수없음";
    }
}

// 상태이상 처리
int CalcPlayerDamage(int base, Enemy* e, Player* p)
{
    int dmg = base;

    // 약화: 주는 피해 감소
    if (p->weak > 0) {
        dmg = dmg * 65 / 100; // Reduce damage by 35%
    }

    // 취약: 받는 피해 증가 (원문 그대로 유지)
    if (e->vuln > 0) {
        dmg = dmg * 130 / 100; // Increase damage by 35%
    }

    // 독: 공격 시 추가 피해
    if (e->poison > 0) {
        dmg += e->poison;
    }

    return (dmg < 0) ? 0 : dmg;
}

// --------------------
// 일지 대사
// --------------------
const char* JOURNAL_TEXT[FLOORS][4][160] = {
    // ===== 1층 =====
    {
        {
            "일지1",
            "",
             "혹여나 이곳의 발을 들인자를 위해 이 글을 작성한다. 이곳의 전투는 단순한 힘겨루기가 아니다.",
             "",
             "전투가 시작되면, 나와 적은 번갈아 행동한다.",
              "",
             "한 턴마다 주어지는 에너지는 제한적이다.",
              "",
             "행동에는 에너지가 필요하다. 공격, 방어, 기술 사용 모두 에너지를 소모한다.",
              "",
             "에너지가 바닥나면, 그 턴에는 더 이상 아무것도 할 수 없다.",
              "",
             "방어는 일시적으로 피해를 막아주지만, 턴이 끝나면 사라진다.",
              "",
             "중독, 화상, 약화, 취약…, 보이지 않는 상처가 전투를 잠식한다.",
              "",
              "약화는 피해를 줄여주며, 취약은 피해를 더해준다.",
              "",
             "상태 이상을 무시하는 순간, 전투는 이미 기울어 있다.",
              "",
             "모든 스킬이 정답은 아니다. 지금 필요한 것이 무엇인지 판단하라.",
              "",
             "마지막으로 한가지 도망은 비겁함이 아니다.",

              NULL   
        },
        {
            "1층 일지2",
            "",
            "던전 곳곳에는 기묘한 표식이 남아 있다.",
            "",
            "그 앞에 서면, 잠시 숨을 고를 수 있다.",
            "",
            "네가 이 던전에 발을 들인 이상",
            "",
            "체크포인트는 돌아갈 수 있는 마지막 지점이다.",
            "",
            "이곳에서 죽는다는 것은, 모든 것이 끝났다는 뜻은 아니다.",
            "",
            "또한 강적 주위엔 가끔식 일반 몬스터가 있을수있다.",
            NULL
        },
        {
            "1층 일지3",
             "",
             "상인은 던전 내부에 유일한 동반자다.",
             "",
             "적어도 뒤통수 칠 일은 없으니",
             "",
             "어쩌면 동료보다 믿음직 하다.",
             "",
             "상인이 파는 물약은 상처를 치료해주고",
             "",
             "떄로는 방어구를 씌워 주기도 한다.",
             "",
             "핵심은 상인이 파는 유물이다",
             "",
             "총 3개까지 구매가능하며, 절대로 되돌릴수 없다.",
             "",
             "고층으로 갈수록 더 높은 효과를 가진 유물이 나올수도 있다.",
             "",
             "그렇지만 선택은 자유다, 미리 3개를 채우든 미래를 보든.",
             "",
            NULL
        }
    },
    // ===== 2층 =====
    {
        { "2층 일지 1",
          
          "",
          "나는 시체를 들고 들어왔다.",
          "",
          "단순히 흔적을 지우기 위해서.",
          "",
          "하지만 이 던전은 흔적을 먹는다.",
          "",
          "피, 땀, 공포… 전부 연료다.",
          "",
          "가끔 벽이 숨을 쉬는 것처럼 보인다.",
          "",
          "그럴 때는 뒤돌아보지 마라.",
          "",
          "뒤돌아보는 순간, 네가 '먹잇감'이 된다.",
          "",
          "심장을 꺼내면 끝날 줄 알았지.",
          "",
          "심장을 들고 나가면 전부 끝일줄 알았지.",
          "",
         
      
          NULL
        },
       {
         "2층 일지 2",
         "",
         "벽이 숨을 쉰다.",
         "",
         "불이 흔들릴 때마다",
         "",
         "그림자가 늦게 따라온다.",
         "",
         "내 그림자가 아니다.",
         "",
         "여기서는 멈춰 서면 안 된다.",
         "",
         "멈춘 순간",
         "",
         "무언가가 나를 인식한다.",
         "",
         "걸을 때는 안전하다.",
         "",
         "생각할 때가 가장 위험하다.",
         NULL
         },

        { "2층 일지 3",
          "",
         "목걸이를 처음 봤을 때",
         "",
         "나는 운명이라 생각했다.",
         "",
         "하지만 운명은 선택지가 아니었다.",
         "",
         "단지 변명일 뿐이었다.",
         "",
         "애초에 목걸이를 보지 않았더라면...",
         "",
         "그렇지만 그게 가능했을까?",
         "",
         "네가 너를 모른 척할 수 있느냐는 거다.",
         "",
         "그뿐만이 아니다, 일종의 운명이다.",
         "",
         NULL }
    },
    // ===== 3층 =====
    {
        { "3층 일지 1",
          "",
          "그건 괴물이 아니다.",
          "",
          "그건 '형태'다.",
          "",
          "수많은 죽음이 겹쳐 만들어진 껍질.",
          "",
          "그래서 심장이 필요하다.",
          "",
          "심장이 없으면, 던전은 문을 열지 않는다.",
          "",
          "문이 열려도, 너는 나가지 못한다.",
          "",
          "…너는 이미 안에 들어와 버렸으니까.",

        NULL },
        { "3층 일지 2",
         "",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "실패했다,실패했다,실패했다,실패했다,실패했다,",
         "",
        NULL },
        { "3층 일지 3",
         "",
         "처음에는 탈출하려 했다.",
         "",
         "그 다음엔",
         "",
         "끝내려고 했다.",
         "",
         "끝나려고 했다.",
         "",
         "왜 여기까지 왔는지 모르겠다.",
         "",
         "문은 열려 있다.",
         "",
         "하지만 나가는 건",
         "",
         "들어오는 것보다",
         "",
         "훨씬 어려운 일이다.",
         "",
        NULL
            },
          },
       {
        { "일지",
        "",
        "목도&라 그것$% 강*&%림#$.",
        "",
        "신%#$% 시작@$@점",
        NULL },
        { NULL },
        { NULL }
      }
    
};


// --------------------
// 콘솔 유틸
// --------------------
char dirchar(Dir d)
{
    switch (d) {
    case NORTH: return '^';
    case EAST:  return '>';
    case SOUTH: return 'v';
    case WEST:  return '<';
    }
    return '?';
}

void waitenter()
{
    int eh;
    while (1) {
        DB_Present();
        eh = _getch();
        if (eh == 13) break;
    }
}

void waitenter_console()
{
    int ch;
    while (1) {
        ch = _getch();
        if (ch == 13) break; // Enter
    }
}

static void NarrHeader(void)
{
    DB_BeginFrame();

    printf("\n");
    printf("                                                 ┌─────────────────────────────┐\n\n");
    printf("                                                 │                         NARRATION                        │\n\n");
    printf("                                                 └─────────────────────────────┘\n\n");
}

void showCursor(int show)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = show ? TRUE : FALSE;
    SetConsoleCursorInfo(h, &info);
}
// 1 또는 2를 반환
int NarrChoice_1or2(const char* title, const char* opt1, const char* opt2)
{
    while (1) {
        NarrHeader();
        printf("                                                 %s\n\n", title);
        printf("                                                 1) %s\n", opt1);
        printf("                                                 2) %s\n\n", opt2);
        printf("                                                 선택: 1 / 2\n");

        DB_Present();
        int ch = _getch();

        if (ch == '1') return 1;
        if (ch == '2') return 2;
    }
}

void Ending_GoodExit(Player* p)
{
    NarrHeader();
    printf("                                                 [Enter] 다음\n\n");
    waitenter();
    printf("                                                 당신은 주먹을 내렸다.\n\n");
    waitenter();
    printf("                                                 …이게 무슨 짓인가 싶었다.\n\n");
    waitenter();
    printf("                                                 애초에 말이 안 된다. 노숙자에게 저런 목걸이라니.\n\n");
    waitenter();
    printf("                                                 괜한 봉변을 당할수도 있다.\n\n");
    waitenter();
    printf("                                                 어쩌면, 신이 내게 선택지를 준게 아닐까?.\n\n");
    waitenter();
    printf("                                                 노숙자는 떨리는 손으로, 말 대신 고개를 숙였다.\n\n");
    waitenter();
    printf("                                                 당신은 등을 돌려 골목을 빠져나왔다.\n\n");
    waitenter();
    printf("                                                 달빛이 더 이상 눈부시지 않았다.\n\n");
    waitenter();
    printf("                                                 허나 그 목걸이의 빛만은, 끝내 머릿속에서 사라지지 않았다.\n\n");
    waitenter();
    printf("                                                 「그날 이후, 당신은 다시 술집을 전전하였다, 어디서나 볼 법한 모험가의 모습으로」\n\n");
    waitenter();

    // 여기서 게임 종료 or 타이틀로
    NarrHeader();
    printf("                                                 --- ENDING: 돌아섬 ---\n\n");
    printf("                                                 아무 키를 누르면 종료합니다.\n");
    DB_Present();
    (void)_getch();
    exit(0);
}

void drawbox(int x, int y, int w, int h, const char* title)
{
    gotoxy(x, y);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    for (int r = 1; r < h - 1; r++) {
        gotoxy(x, y + r);
        putchar('|');
        gotoxy(x + w - 1, y + r);
        putchar('|');
    }

    gotoxy(x, y + h - 1);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    if (title && title[0]) {
        gotoxy(x + 2, y);
        printf("%s", title);
    }
}

int readKey()
{
    int ch = _getch();

    // 확장키(방향키 등) 처리 필요하면:
    if (ch == 0 || ch == 224) ch = _getch();

    return ch;
}
void startback()
{
    NarrHeader();
    printf("                                                 화면을 확대/축소시 깨질수 있으며, 이떄 나래이션 부분을 재외한 화면에서\n\n");
    printf("                                                 R 키를 눌러 화면을 재정렬 해주세요.\n\n");
    printf("                                                 처음 타이틀과 나래이션 부분에선 화면을 확대하지 마시고.\n\n");
    printf("                                                 던전에 들어간 다음 확대 후 R 키를 눌러주세요.\n\n");
    printf("                                                 !!이름을 한글로 작성했을시 이동과 기능작동이 안될수 있습니다!!그럴떈 한 / 영키를 눌러주세요\n\n");
    printf("                                                 그럼 행운을 빕니다.\n\n");
    printf("                                                 [Enter] 다음\n\n");
    waitenter();
}
void narrationend()
{
    DB_BeginFrame();
    NarrHeader();
    printf("                                                 [Enter] 다음\n\n");
    waitenter();
    printf("                                                「죽였다」 \n\n");
    waitenter();
    printf("                                                 당신은 ???을 물리쳤다.\n\n");
    waitenter();
    printf("                                                 그렇지만 귀환석은 나타나지 않았다.\n\n");
    waitenter();
    printf("                                                「왜 나타나지 귀환석이 않는거지?」 \n\n");
    waitenter();
    printf("                                                「이제 돌아가서 이 목걸이만 팔아넘기면 다 끝인데 왜!?」 \n\n");
    waitenter();
    printf("                                                 던전은 떨린다, 이내 문이 열린다. \n\n");
    waitenter();
    printf("                                                 당신은 앞으로 나아간다. \n\n");
    waitenter();
    printf("                                                「.....아」 \n\n");
    waitenter();
    printf("                                                 어서 목숨을 끊는다. \n\n");
    waitenter();
    printf("                                                 'Q' \n\n");
    waitenter();
    DB_Present();
}

void showStartScreen()
{
    while (1) {
        // 프레임 시작: 여기서 창크기/오프셋 갱신 + 백버퍼 clear 됨


        DB_BeginFrame();

        printf("\n\n");
        for (int i = 0; i < 160; i++) printf("=");
        printf("\n\n");

        printf(
            "                                         |||||   ||   ||   |||||   ||      |||||   ||   ||   ||||||  ||   |||||  ||||||\n"
            "                                         ||  ||  ||   ||  ||   ||  ||     ||   ||  |||  ||  ||       ||  ||        ||\n"
            "                                         |||||   |||||||  |||||||  ||     |||||||  || | ||  ||  |||  ||   |||||    ||\n"
            "                                         ||      ||   ||  ||   ||  ||     ||   ||  ||  |||  ||   ||  ||       ||   ||\n"
            "                                         ||      ||   ||  ||   ||  |||||  ||   ||  ||   ||   ||||||  ||   |||||    ||\n\n\n"
        );

        for (int i = 0; i < 160; i++) printf("=");
        printf("\n\n");
        printf("                                                         [Enter] start   [R] redraw   [ESC] quit\n");

        DB_Present();

        int ch = _getch();
        if (ch == 13) {          // Enter
            return;              // 타이틀 종료 -> 게임 진행
        }
        if (ch == 27) {          // ESC
            exit(0);
        }
        if (ch == 'r' || ch == 'R') {
            // 그냥 루프 계속 돌면 다음 반복에서 DB_BeginFrame이
            // 창크기 갱신 + 다시 그리기 해줌
            continue;
        }
    }
}


void ClearPendingInputSafe()
{
    while (_kbhit()) {
        int c = _getch();
        if (c == 224 || c == 0) {
            if (_kbhit()) (void)_getch();
        }
    }
}

static void ConsolePrint(const char* fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    DWORD written;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)strlen(buf), &written, NULL);
}

void showNarration(Player* p)
{
    PlaySound(TEXT("sound/bgm/dark_outside.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
    // ---- 1페이지 ----
    
    NarrHeader();
    printf("                                                 한적한 새벽, 당신은 술집에서 나와 골목길을 걷고 있다.\n\n");
    waitenter();
    printf("                                                 술의 취해 휘청거리던 당신은 누군가의 다리에 걸려 넘어졌다.\n\n");
    waitenter();
    printf("                                                 그 다리의 주인은 지저분한 차림새의 한 노숙자였다.\n\n");
    waitenter();
    printf("                                                 당신의 화는 이내 그 노숙자를 향헀다.\n\n");
    waitenter();
    printf("                                                 당신은 노숙자의 멱살을 잡고 벽에 밀치고, 주먹을 높게 들었다.\n\n");
    waitenter();
    printf("                                                 그 순간, 당신은 보았다.\n\n");
    waitenter();
    printf("                                                 노숙자의 차림새와 어울리지 않게 달빛을 반사하며 광택을 흩뿌리는 목걸이를.\n\n");
    waitenter();
    printf("                                                 당신은 노숙자의 목걸이를 뺴앗아 달아나고 싶었다.\n\n");
    waitenter();
    printf("                                                 하지만 그러기엔 오늘 아침 광장에서 들었던 한 소식이 생각에 걸렸다.\n\n");
    waitenter();
    printf("                                                 국가의 재정 문제로, 죄에 따른 형벌이 늘어났다는 이야기가.\n\n");
    waitenter();
    printf("                                                 죄를 짓지 않아도 얼마든지 목이 날아갈 수 있다.\n\n");
    waitenter();
    printf("                                                 단순한 절도만으로도 감옥에서 썩기엔 충분했다. 물론 약간의 폭력도.\n\n");
    waitenter();
    printf("                                                 하지만... 목격자는 없다.\n\n");
    waitenter();
    printf("                                                 목걸이를 챙기고 이 마을을 떠나면, 그걸로 끝이다.\n\n");
    waitenter();
    printf("                                                 노인이 무슨 말을 한들, 그 말을 들어줄 사람은 없을 것이다.\n\n");
    waitenter();
    printf("                                                 그런 생각이 머릿속을 스치는 순간, 노숙자가 소리쳤다.\n\n");
    waitenter();
    // ---- 2페이지 ----
    NarrHeader();
    printf("                                                 [Enter] 다음\n\n");
    waitenter();
    printf("                                                「..으...아ㅡ!!」\n\n");
    waitenter();
    printf("                                                「 으..으...아ㅡ!!」\n\n");
    waitenter();
    printf("                                                 노숙자의 외침을 들은 당신은 생각했다.\n\n");
    waitenter();
    printf("                                                 ........\n\n");
    waitenter();
    printf("                                                 노숙자는 벙어리다.\n\n");
    waitenter();
    printf("                                                 ........\n\n");
    waitenter();
    int c = NarrChoice_1or2(
        "........",
        "목걸이를 뺏는다",
        "주먹을 내린다"
    );

    if (c == 2) {
        Ending_GoodExit(p);
        return; // 혹시 몰라서
    }
    printf("                                                 당신의 입가에 미소가 번졌다.\n\n");
    waitenter();
    printf("                                                 당신의 높은 주먹은 이내 노숙자의 목걸이를 향해 뻗어갔다.\n\n");
    waitenter();
    printf("                                                 노숙자는 필사적으로 저항했지만, 이내 목걸이를 빼앗기고 바닥을 나뒹굴렀다.\n\n");
    waitenter();
    printf("                                                「..으...악!!.」\n\n");
    waitenter();
    printf("                                                 목걸이를 잃은 순간, 노숙자는 뭔가 끊어진 듯했다.\n\n");
    waitenter();
    printf("                                                 그 순간, 노숙자가 달려들었다.\n\n");
    waitenter();
    printf("                                                「...그아!!!...」\n\n");
    waitenter();
    printf("                                                 당신의 머리에선 피가 떨어졌고, 노숙자의 손엔 피 묻은 돌이 들려 있었다.\n\n");
    waitenter();
    printf("                                                 잠시 후, 당신은 정신을 차렸다.\n\n");
    waitenter();
    printf("                                                 눈앞의 노숙자는 쓰려져있었고 당신의 손에는 마찬가지로 피 묻은 돌이 쥐어져 있었다.\n\n");
    waitenter();
    printf("                                                 「…씨발。」\n\n");
    waitenter();
    printf("                                                 당신은 노숙자를 죽였다.\n\n");
    waitenter();
    NarrHeader();
    printf("                                                 「죽이려고 한 건 아니었는데…」\n\n");
    waitenter();
    printf("                                                 식은땀이 흐르며 몸이 굳어, 생각조차 제대로 할 수 없었다.\n\n");
    waitenter();
    printf("                                                 날이 밝으면 난 잡힐거다. 반드시.\n\n");
    waitenter();
    printf("                                                 알리바이는 충분하다 술집 맞음편 골목길 거기다 난 오늘 그 술집의 마지막 손님이다.\n\n");
    waitenter();
    printf("                                                 알리바이 따위는 없어도 된다. 정부는 충분히 만들수 있으니까.\n\n");
    waitenter();
    printf("                                                 또한 지금 당장이라도 순찰중인 경비에게 잡힐수 있다.\n\n");
    waitenter();
    printf("                                                 그떄, 당신의 시선이 벽면에 붙은 한 장의 의뢰서에 멈췄다.\n\n");
    waitenter();
    // ---- 3페이지 ----
    NarrHeader();
    printf("                                                 -의뢰- \n");
    waitenter();
    printf("                                                 -중급 던전 팔랑크스- \n\n");
    waitenter();
    printf("                                                  의뢰 조건: 던전의 주인 '팔랑크스' 처치, 심장을 증거로 제출할 것.\n\n");
    waitenter();
    printf("                                                「...그래. 여기로 가자」\n\n");
    waitenter();
    printf("                                                 중급 던전 임에도 생존자가 없어 저주 받은 던전이라 불리우는 곳, 즉 아무도 올사람이 없다는 것이다.\n");
    waitenter();
    printf("                                                 모험가들 사이에선 예로부터 시체를 은폐하는 데 던전만 한 곳은 없다는 말이 떠돌았다.\n\n");
    waitenter();
    printf("                                                 당신은 곳바로 노숙자의 시체를 숨겨 집으로와 그곳에 갈 준비를 한다.\n\n");
    waitenter();
    printf("                                                「그걸 진짜로 하게 될 줄이야.. 」\n\n");
    waitenter();
   

    // 이름 입력 페이지
   // DB로 그린 마지막 화면이 남아있을 수 있으니 한번 뿌리고
    DB_Present();
    
    // 콘솔 모드로 전환
    showCursor(1);
    system("cls");

    ConsolePrint("\n");
    ConsolePrint("┌──────────────────────────────────────────────────────────┐\n");
    ConsolePrint("│                         NARRATION                        │\n");
    ConsolePrint("└──────────────────────────────────────────────────────────┘\n\n");
    ConsolePrint(" 어떤 의뢰를 수주하시나요?\n\n");
    waitenter_console();
    ConsolePrint("....팔랑크스\n\n");
    waitenter_console();
    ConsolePrint("네 알겠습니다. \n\n");
    waitenter_console();
    ConsolePrint("이름이 어떻게되시죠?. \n\n");
    waitenter_console();
    ConsolePrint("이름을 입력하시오: \n\n");
    // 입력 버퍼 정리
    
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hIn);   // ← 여기서 한 번만 비우고

    fgets(p->name, sizeof(p->name), stdin);
    p->name[strcspn(p->name, "\n")] = '\0';
    if (p->name[0] == '\0') strcpy_s(p->name, sizeof(p->name), "방랑자");

    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    showCursor(0);

    // 다시 DB 화면으로 복귀할 준비
    DB_BeginFrame();

    // ---- 5페이지 ----
    NarrHeader();
    printf("                                                 %s은(는) 홀로 던전 앞에 도착했다.\n\n", p->name);
    waitenter();
    printf("                                                 등에 들쳐맨 시체와 함께\n\n");
    waitenter();
    printf("                                                 던전은 클리어 하기 전까진 나갈수 없다.\n\n");
    waitenter();
    printf("                                                 %s은(는) 마음을 한차례 심호흡을 한 후 커다란 문을 열고 앞으로 나아간다.\n\n", p->name);
    waitenter();
    printf("                                                 그 심호흡은 썩어가는 피에 찌든 악취를 동반헀다.\n\n");
    waitenter();
}

void showNarration2(Player* p)
{
    NarrHeader();
    printf("                                                 「…끝난 건가」\n\n");
    waitenter();
    printf("                                                 심장이 아직도 빠르게 뛰고 있었다.\n\n");
    waitenter();
    printf("                                                 숨을 고르려 했지만, 공기가 잘 들어오지 않았다.\n\n");
    waitenter();
    printf("                                                 그 순간, 던전 깊은 곳에서 무언가 움직이는 소리가 들렸다.\n\n");
    waitenter();
    printf("                                                 뒤를 돌아보았지만,\n\n");
    waitenter();
    printf("                                                 들어왔던 길은 어둠 속에 묻혀 있었다.\n\n");
    waitenter();
    printf("                                                 「…하, 선택지는 하나뿐이군 뭐 애초에 던전 주인을 잡기전엔 나갈수 없으니까」\n\n");
    waitenter();
    printf("                                                 「시체는 진작에 처리했고, 이제 그 심장만 얻으면 되겠네」\n\n");
    waitenter();
    printf("                                                 벽이 갈라지며, 아래로 이어진 계단이 모습을 드러냈다.\n\n");
    waitenter();
    printf("                                                 더 눅눅하고, 더 무거운 공기가 흘러나왔다.\n\n");
    waitenter();
    printf("                                                 「여긴… 아직 입구였다는 거지」\n\n");
    waitenter();
    printf("                                                 당신은 피 묻은 무기를 고쳐 쥐고, 계단을 내려갔다.\n\n");
    waitenter();
    ClearPendingInputSafe();

}

void setConsoleSize(int width, int height)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufferSize = { (SHORT)width, (SHORT)height };
    SetConsoleScreenBufferSize(h, bufferSize);
    SMALL_RECT windowSize = { 0, 0, (SHORT)(width - 1), (SHORT)(height - 1) };
    SetConsoleWindowInfo(h, TRUE, &windowSize);
}

void clearBoxInner(int x, int y, int w, int h)
{
    for (int r = 1; r < h - 1; r++) {
        gotoxy(x + 1, y + r);
        for (int i = 0; i < w - 2; i++) putchar(' ');
    }
}

void ClearScreenNoFlicker()
{
    // system("cls")를 피하려고 만든 함수였던 듯.
    // 실제 지우기는 백버퍼로 처리중이므로 여기서는 큰 의미 없지만 유지.
    DB_BeginFrame();
}

void clearArea(int x, int y, int w, int h)
{
    for (int r = 0; r < h; r++) {
        gotoxy(x, y + r);
        for (int i = 0; i < w; i++) putchar(' ');
    }
}

void drawUIFrame()
{
    ClearScreenNoFlicker();

    drawbox(0, 0, UI_W, UI_H, "");
    drawbox(M_X, M_Y, M_W, M_H, "");
    drawbox(MSG_X, MSG_Y, MSG_W, MSG_H, " 메세지 ");
    drawbox(STAT_X, STAT_Y, STAT_W, STAT_H, " 상태 ");
    drawbox(DECK_BOX_X, DECK_BOX_Y, DECK_BOX_W, DECK_BOX_H, " 스킬북 ");

    clearBoxInner(M_X, M_Y, M_W, M_H);
    clearBoxInner(MSG_X, MSG_Y, MSG_W, MSG_H);
    clearBoxInner(STAT_X, STAT_Y, STAT_W, STAT_H);
    clearBoxInner(DECK_BOX_X, DECK_BOX_Y, DECK_BOX_W, DECK_BOX_H);


}

void renderMessageLine(int line, const char* msg)
{
    int y = MSG_Y + 2 + line;
    gotoxy(MSG_X + 2, y);
    printf("%-66s", msg ? msg : "");
}
const char* pickRandomLine()
{
    static const char* lines[] = {
        "음습한 던전 속에서 고요함이 느껴진다...",
        ".....",
        "어둠 속에서 청각이 예민해진 기분이다..",
         ".....",
        "이 길이 맞는 것 같은 느낌이든다.",
         ".....",
        "뭔가 수상한 기운이 느껴진다.",
         ".....",
        "무언의 압박감이 느껴진다...",
         ".....",
        "발소리가 들린다.",
         ".....",
        "떨리는 심장의 파동이 온 몸을 관통한다.",
         ".....",
        "정적 속에서 심장 소리만이 또렷이 들린다."
         ".....",
    };
    int count = (int)(sizeof(lines) / sizeof(lines[0]));
    return lines[rand() % count];
}
void renderMessage(const char* msg)
{
    renderMessageLine(0, msg);
}

void CodexNotice(const char* msg)
{
    int x = CODEX_X;
    int y = CODEX_Y + CODEX_ROWS * SLOT_H + (CODEX_ROWS - 1) * SLOT_GAP_Y + 1;
    if (y > UI_H - 3) y = UI_H - 3;

    gotoxy(x, y);
    printf("%-150s", "");
    gotoxy(x, y);
    printf("%-150s", msg ? msg : "");
}

void DrawPassiveUI(Player* p)
{
    int x = STAT_X + 2;
    int y = STAT_Y + 9;   // 상태창 아래쪽 (기존 STAT_H=10 기준)

    gotoxy(x, y);
    printf("유물");

    for (int i = 0; i < MAX_PASSIVE; i++) {
        gotoxy(x, y + 1 + i);
        if (p->passiveSlots[i] == -1) {
            printf("[%d] (비어있음)        ", i + 1);
        }
        else {
            printf("[%d] %-16s", i + 1, GetPassiveName(p->passiveSlots[i]));
        }
    }
}

void playerstate(Player* p)
{
    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%-22s", "");
    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%s", p->name);

    gotoxy(STAT_X + 2, STAT_Y + 3);
    printf("레벨 %d", p->level);

    gotoxy(STAT_X + 2, STAT_Y + 5);
    printf("경험치 %d/%d", p->exp, p->expToNext);

    gotoxy(STAT_X + 2, STAT_Y + 6);
    printf("체력 %d/%d ", p->HP, p->maxHP);

    gotoxy(STAT_X + 2, STAT_Y + 7);
    printf("골드 %d ", p->gold);
}

void renderSkillBookObject()
{
    int x = DECK_BOX_X + 3;
    int y = DECK_BOX_Y + 2;

    gotoxy(x, y + 0);  printf("   ______________");
    gotoxy(x, y + 1);  printf("  /___________/||");
    gotoxy(x, y + 2);  printf(" +------------+||");
    gotoxy(x, y + 3);  printf(" |            |||");
    gotoxy(x, y + 4);  printf(" |    sKill   |||");
    gotoxy(x, y + 5);  printf(" |            |||");
    gotoxy(x, y + 6);  printf(" |            |||");
    gotoxy(x, y + 7);  printf(" |            |||");
    gotoxy(x, y + 8);  printf(" |            |||");
    gotoxy(x, y + 9);  printf(" |            |||");
    gotoxy(x, y + 10); printf(" +------------+/");
}

// --------------------
// 패시브/물약 유틸
// --------------------
int HasPassive(Player* p, int passiveId)
{
    for (int i = 0; i < MAX_PASSIVE; i++)
        if (p->passiveSlots[i] == passiveId) return 1;
    return 0;
}

int AddPassive(Player* p, int passiveId)
{
    if (HasPassive(p, passiveId)) return 0;            // 중복
    if (p->passiveCount >= MAX_PASSIVE) return -1;     // 꽉참

    for (int i = 0; i < MAX_PASSIVE; i++) {
        if (p->passiveSlots[i] == -1) {
            p->passiveSlots[i] = passiveId;
            p->passiveCount++;
            return 1;
        }
    }
    return -1;
}

// 패시브 이름/설명

void PrintPassives(Player* p, int x, int y)
{
    gotoxy(x, y);     printf("패시브(%d/%d):", p->passiveCount, MAX_PASSIVE);
    for (int i = 0; i < MAX_PASSIVE; i++) {
        gotoxy(x, y + 1 + i);
        if (p->passiveSlots[i] == -1) printf("%d) (비어있음)        ", i + 1);
        else printf("%d) %s            ", i + 1, GetPassiveName(p->passiveSlots[i]));
    }
}

const char* GetPassiveDesc(int id)
{
    switch (id) {
    case 0: return "최대 체력 +15 (즉시)";
    case 1: return "전투 시작 시 방어막 +8";
    case 2: return "최대 에너지 +1 (전투 에너지 상한 증가)";
    case 3: return "전투 승리 시 골드 추가 +9";
    case 4: return "독/화상 부여량 +1";
    case 5: return "0비용 스킬 턴당 사용 제한이 1->2";
    case PASSIVE_GRIT_HEART:
        return "전투 중 HP 0 도달 시 1회, 최대체력 30%로 회복";
    case PASSIVE_FINAL_PREP:
        return "전투 시작 첫 턴에 에너지 +2";
    default: return "";
    }
}
// 즉시적용
void ApplyPassiveImmediate(Player* p, int id)
{
    if (id == 0) { // 최대체력 +15
        p->maxHP += 15;
        p->HP += 15;
        if (p->HP > p->maxHP) p->HP = p->maxHP;
    }
}
// 전투시작보호막(방어막 등)
int PassiveBattleStartBlock(Player* p)
{
    int bonus = 0;
    for (int i = 0; i < MAX_PASSIVE; i++) {
        int id = p->passiveSlots[i];
        if (id == -1) continue;
        if (id == 1) bonus += 8; // 전투 시작 방어막 +8
    }
    return bonus;
}
// 에너지 최대치(패)
int PlayerEnergyMax(Player* p)
{
    int max = ENERGY_MAX;
    for (int i = 0; i < MAX_PASSIVE; i++) {
        int id = p->passiveSlots[i];
        if (id == -1) continue;
        if (id == 2) max += 1; // 에너지 +1
    }
    return max;
}
// 상태이상 보너스
int PassiveStatusBonus(Player* p)
{
    int b = 0;
    for (int i = 0; i < MAX_PASSIVE; i++) {
        int id = p->passiveSlots[i];
        if (id == -1) continue;
        if (id == 4) b += 1; // 독/화상 +1
    }
    return b;
}
//전투 후 골드
int PassiveKillGoldBonus(Player* p)
{
    int b = 0;
    for (int i = 0; i < MAX_PASSIVE; i++) {
        int id = p->passiveSlots[i];
        if (id == -1) continue;
        if (id == 3) b += 9;
    }
    return b;
}
// 0코 스킬 던당 제한(기본1 -> 2)
int PassiveZeroCostLimit(Player* p)
{
    int limit = 1;
    for (int i = 0; i < MAX_PASSIVE; i++) {
        int id = p->passiveSlots[i];
        if (id == -1) continue;
        if (id == 5) limit = 2;
    }
    return limit;
}
// 물약(맵에서만 사용가능하다)
void OpenPotionMenu(Player* p)
{
    while (1) {
        DB_BeginFrame();
        drawbox(20, 8, 120, 28, " 물약 (비전투시 사용가능) ");

        gotoxy(24, 12); printf("1) 회복물약(+25)   보유:%d", p->potions[POT_HEAL25]);
        gotoxy(24, 14); printf("2) 상급회복(+50)   보유:%d", p->potions[POT_HEAL50]);
        gotoxy(24, 16); printf("3) 방어물약(방어막+10 다음 전투) 보유:%d", p->potions[POT_GUARD10]);
        gotoxy(24, 18); printf("4) 상급방어물약(방어막+30 다음 전투) 보유:%d", p->potions[POT_GUARD30]);

        gotoxy(24, 22); printf("0) 나가기");
        DB_Present();

        int ch = _getch();
        if (ch == '0' || ch == 27) return;

        if (ch == '1' && p->potions[POT_HEAL25] > 0) {
            p->potions[POT_HEAL25]--;
            p->HP += 25; if (p->HP > p->maxHP) p->HP = p->maxHP;
            renderMessage("회복물약 사용! (아무 키)");
            wait_any_key();
        }
        else if (ch == '2' && p->potions[POT_HEAL50] > 0) {
            p->potions[POT_HEAL50]--;
            p->HP += 50; if (p->HP > p->maxHP) p->HP = p->maxHP;
            renderMessage("상급 회복물약 사용! (아무 키)");
            wait_any_key();
        }
        else if (ch == '3' && p->potions[POT_GUARD10] > 0) {
            p->potions[POT_GUARD10]--;
            p->altarGuardBonus += 10; // 기존 변수 재활용: 다음 전투 시작 방어막
            renderMessage("방어물약 사용! 다음 전투 방어막 +10 (아무 키)");
            wait_any_key();
        }
        else if (ch == '4' && p->potions[POT_GUARD30] > 0) {
            p->potions[POT_GUARD30]--;
            p->altarGuardBonus += 30; // 기존 변수 재활용: 다음 전투 시작 방어막
            renderMessage("상급 방어물약 사용! 다음 전투 방어막 +30 (아무 키)");
            wait_any_key();
        }
        else {
            renderMessage("사용할 수 없다(보유 0). 아무 키.");
            wait_any_key();
        }
    }
}
// 물약 판매 가격
int PotionPrice(PotionType t)
{
    switch (t) {
    case POT_HEAL25:   return 15;
    case POT_HEAL50:   return 30;
    case POT_GUARD10: return 20;
    case POT_GUARD30:  return 30;
    default: return 999;
    }
}

const char* PotionName(PotionType t)
{
    switch (t) {
    case POT_HEAL25:   return "회복물약(+25)";
    case POT_HEAL50:   return "상급회복(+50)";
    case POT_GUARD10: return "방어물약(다음전투 방어+10)";
    case POT_GUARD30:  return "상급방어물약(다음전투 방어+30)";
    default: return "???";
    }
}
// 상점 화면
void OpenShop(Player* p)
{
    while (1) {
        DB_BeginFrame();
        drawbox(10, 4, 140, 36, " 상인 ");

        gotoxy(14, 7);
        printf("골드: %d", p->gold);

        // 패시브(층당 2개)
        gotoxy(14, 9);
        printf("== 유물(최대 3개 장착) ==");

        for (int i = 0; i < 2; i++) {
            int pid = PASSIVE_BY_FLOOR[p->floor][i].passiveId;
            int prc = PASSIVE_BY_FLOOR[p->floor][i].price;
            gotoxy(16, 11 + i * 2);
            printf("%d) %-28s [%dG]%s", i + 1, GetPassiveName(pid), prc,
                HasPassive(p, pid) ? " (보유중)" : "");
            gotoxy(20, 12 + i * 2);
            printf("- %s", GetPassiveDesc(pid));
        }

        // 물약
        gotoxy(14, 16);
        printf("== 물약(제한X , 전투상태가 아닐때만 사용가능) ==");

        for (int i = 0; i < POT_COUNT; i++) {
            gotoxy(16, 18 + i);
            printf("%d) %-28s [%dG] 보유:%d",
                3 + i, PotionName((PotionType)i), PotionPrice((PotionType)i), p->potions[i]);
        }

        // 현재 패시브 표시
        gotoxy(90, 9);
        PrintPassives(p, 90, 10);

        gotoxy(14, 32);
        printf("선택: 1~2(유물) / 3~6(물약) | ESC/0: 나가기");

        DB_Present();

        int ch = _getch();
        if (ch == 27 || ch == '0') {
            ClearPendingInputSafe();
            return;
        }
        

        // ---- 패시브 구매 ----
        if (ch == '1' || ch == '2') {
            int idx = ch - '1';
            int pid = PASSIVE_BY_FLOOR[p->floor][idx].passiveId;
            int prc = PASSIVE_BY_FLOOR[p->floor][idx].price;

            if (HasPassive(p, pid)) {
                renderMessage("이미 가진 유물이다. (아무 키)");
                wait_any_key();
                continue;
            }
            if (p->passiveCount >= MAX_PASSIVE) {
                renderMessage("유물 슬롯이 가득 찼다(최대 3개). (아무 키)");
                wait_any_key();
                continue;
            }
            if (p->gold < prc) {
                renderMessage("골드가 부족하다. (아무 키)");
                wait_any_key();
                continue;
            }

            p->gold -= prc;
            AddPassive(p, pid);
            ApplyPassiveImmediate(p, pid);

            renderMessage("유물 구매/장착 완료! (아무 키)");
            wait_any_key();
            continue;
        }

        // ---- 물약 구매 (3~6) ----
        if (ch >= '3' && ch <= '6') {
            int pi = (ch - '3'); // 0..3
            PotionType t = (PotionType)pi;
            int prc = PotionPrice(t);

            if (p->gold < prc) {
                renderMessage("골드가 부족하다. (아무 키)");
                wait_any_key();
                continue;
            }

            p->gold -= prc;
            p->potions[t]++;

            renderMessage("물약 구매 완료! (아무 키)");
            wait_any_key();
            continue;
        }
    }
}

// 장착(6개) 유틸
int IsEquipped(Player* p, int skillId)
{
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == skillId) return 1;
    }
    return 0;
}

int FirstEmptyEquipSlot(Player* p)
{
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == -1) return i;
    }
    return -1;
}

void RecountEquip(Player* p)
{
    int c = 0;
    for (int i = 0; i < MAX_EQUIP; i++)
        if (p->equip[i] != -1) c++;
    p->equipCount = c;
}

void UnequipSkill(Player* p, int skillId)
{
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == skillId) {
            p->equip[i] = -1;
            break;
        }
    }
    RecountEquip(p);
}

void EquipSkill(Player* p, int skillId)
{
    if (IsEquipped(p, skillId)) return;

    int slot = FirstEmptyEquipSlot(p);
    if (slot != -1) {
        p->equip[slot] = skillId;
        RecountEquip(p);
        return;
    }

    // 꽉 차 있으면 교체 선택 UI (1~6)
    gotoxy(34, 31);
    printf("%-80s", ""); // 잔상 지우기용(폭 적당히)
    gotoxy(34, 31);
    printf("슬롯이 가득 찼다. 교체할 슬롯(1~6) 입력, ESC 취소");

    DB_Present();
    int ch = _getch();
    if (ch == 27) return;

    if (ch >= '1' && ch <= '6') {
        int idx = ch - '1';
        p->equip[idx] = skillId;
        RecountEquip(p);
    }
}

void DrawEquippedList(Player* p, int x, int y)
{
    gotoxy(x + 4, y + 15);
    printf("장착(전투 사용) 스킬 6개:");
    for (int i = 0; i < MAX_EQUIP; i++) {
        gotoxy(x, y + 1 + i);
        if (p->equip[i] == -1) {
            printf("%d) (비어있음) ", i + 1);
        }
        else {
            Skill* s = &codexSkills[p->equip[i]];
            printf("%d) %-18s(비용:%d) ", i + 1, s->name, s->cost);
        }
    }
    gotoxy(x, y + 8);
    printf("아무 키: 닫기");
}


// 도감
void ShowSkillDetail(Skill s)
{
    ClearScreenNoFlicker();
    drawbox(20, 5, 120, 30, " 스킬 상세 ");

    if (!s.unlocked) {
        gotoxy(24, 10);
        printf("잠김 상태");
        gotoxy(24, 11);
        printf("해금이 필요합니다.");
        gotoxy(24, 28);
        printf("아무 키: 돌아가기");
        wait_any_key();
        return;
    }

    gotoxy(24, 9);  printf("이름 : %s", s.name);
    gotoxy(24, 11); printf("비용 : %d", s.cost);
    gotoxy(24, 13); printf("피해 : %d", s.dmg);
    gotoxy(24, 14); printf("방어 : %d", s.block);
    gotoxy(24, 15); printf("회복 : %d", s.heal);
    gotoxy(24, 18); printf("설명 : %s", s.dec);

    gotoxy(24, 28);
    printf("아무 키: 돌아가기");
    wait_any_key();
}

void DrawCodexSlot(int x, int y, int w, int h, const Skill* s, int hasSkill, int selected, int equipped)
{
    // 테두리
    gotoxy(x, y);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    for (int r = 1; r < h - 1; r++) {
        gotoxy(x, y + r);
        putchar('|');
        gotoxy(x + 1, y + r);
        for (int i = 0; i < w - 2; i++) putchar(' ');
        gotoxy(x + w - 1, y + r);
        putchar('|');
    }

    gotoxy(x, y + h - 1);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    // 선택표시(잔상 제거 포함)
    gotoxy(x - 2, y + h / 2);
    printf("%c", selected ? '>' : ' ');
    gotoxy(x + w + 1, y + h / 2);
    printf("%c", selected ? '<' : ' ');

    // 내용
    if (!hasSkill || !s) {
        gotoxy(x + 2, y + 2);
        printf("%-18s", "(EMPTY)");
        gotoxy(x + 2, y + 6);
        printf("%-6s", " ");
        return;
    }

    if (!s->unlocked) {
        gotoxy(x + 2, y + 2);
        printf("%-18s", "???? (잠김)");
        gotoxy(x + 2, y + 4);
        printf("%-18s", "해금 필요");
        gotoxy(x + 2, y + 6);
        printf("%-6s", " ");
        return;
    }

    gotoxy(x + 2, y + 2); printf("%-18.18s", s->name);
    gotoxy(x + 2, y + 3); printf("비용:%-2d ", s->cost);
    gotoxy(x + 2, y + 4); printf("피:%-3d 방:%-3d ", s->dmg, s->block);
    gotoxy(x + 2, y + 5); printf("회:%-3d ", s->heal);

    gotoxy(x + 2, y + 6);
    printf("%-6s", equipped ? "[장착]" : " ");
}

static void RedrawOneCodexSlot(Player* p, int floorPage, int slotIdx, int selectedSlot)
{
    int startIndex = floorPage * SKILLS_PER_FLOOR;
    int skillIdx = startIndex + slotIdx;

    int x = CODEX_X + (slotIdx % CODEX_COLS) * (SLOT_W + SLOT_GAP_X);
    int y = CODEX_Y + (slotIdx / CODEX_COLS) * (SLOT_H + SLOT_GAP_Y);

    int hasSkill = (skillIdx >= 0 && skillIdx < MAX_SKILLS);
    int equipped = 0;

    if (hasSkill && codexSkills[skillIdx].unlocked)
        equipped = IsEquipped(p, codexSkills[skillIdx].id);

    if (hasSkill)
        DrawCodexSlot(x, y, SLOT_W, SLOT_H, &codexSkills[skillIdx], 1, (slotIdx == selectedSlot), equipped);
    else
        DrawCodexSlot(x, y, SLOT_W, SLOT_H, NULL, 0, (slotIdx == selectedSlot), 0);
}

static void RedrawCodexPage(Player* p, int floorPage, int selected)
{
    clearBoxInner(2, 1, UI_W - 4, UI_H - 2);
    gotoxy(6, 2);
    printf("E/ESC: 닫기 | ENTER: 상세 | R: 장착/해제 | T: 장착목록 | 방향키: 이동 | A/D: 층 페이지 (층 %d) ",
        floorPage + 1);

    for (int i = 0; i < CODEX_SLOTS; i++) {
        RedrawOneCodexSlot(p, floorPage, i, selected);
    }
    CodexNotice(" ");
}

void OpenCodexScene(Player* p)
{
    int floorPage = p->floor;
    int selected = 0;

    drawbox(0, 0, UI_W, UI_H, "");
    drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
    RedrawCodexPage(p, floorPage, selected);
    DB_Present();

    while (1) {
        int ch = _getch();

        if (ch == 224 || ch == 0) {
            int k = _getch();
            int prev = selected;

            int row = selected / CODEX_COLS;
            int col = selected % CODEX_COLS;

            if (k == 72 && row > 0) row--;
            else if (k == 80 && row < CODEX_ROWS - 1) row++;
            else if (k == 75 && col > 0) col--;
            else if (k == 77 && col < CODEX_COLS - 1) col++;

            selected = row * CODEX_COLS + col;

            if (selected != prev) {
                RedrawOneCodexSlot(p, floorPage, prev, selected);
                RedrawOneCodexSlot(p, floorPage, selected, selected);
                CodexNotice(" ");
                DB_Present();
            }
            continue;
        }

        if (ch == 27 || ch == 'e' || ch == 'E') {
            ClearPendingInputSafe();
            return;
        }

        if (ch == 'a' || ch == 'A') {
            if (floorPage > 0) floorPage--;
            selected = 0;
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }
        if (ch == 'd' || ch == 'D') {
            if (floorPage < FLOORS - 1) floorPage++;
            selected = 0;
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }

        if (ch == 't' || ch == 'T') {
            DB_BeginFrame();
            drawbox(10, 5, 140, 30, " 장착 목록 ");
            DrawEquippedList(p, 14, 8);
            wait_any_key();

            drawbox(0, 0, UI_W, UI_H, "");
            drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }

        if (ch == 13) {
            int idx = floorPage * SKILLS_PER_FLOOR + selected;
            if (idx >= 0 && idx < MAX_SKILLS) ShowSkillDetail(codexSkills[idx]);

            drawbox(0, 0, UI_W, UI_H, "");
            drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }

        if (ch == 'r' || ch == 'R') {
            int idx = floorPage * SKILLS_PER_FLOOR + selected;
            if (idx >= 0 && idx < MAX_SKILLS) {
                Skill* s = &codexSkills[idx];
                if (!s->unlocked) {
                    CodexNotice("잠김 스킬은 장착할 수 없다. 아무 키.");
                    wait_any_key();
                }
                else {
                    if (IsEquipped(p, s->id)) {
                        UnequipSkill(p, s->id);
                        CodexNotice("장착 해제됨. 아무 키.");
                    }
                    else {
                        EquipSkill(p, s->id);
                        CodexNotice("장착(또는 교체) 완료. 아무 키.");
                    }
                    wait_any_key();
                }
                RedrawOneCodexSlot(p, floorPage, selected, selected);
                CodexNotice(" ");
                DB_Present();
            }
            continue;
        }
    }
}

// --------------------
// 스킬 DB 초기화
// --------------------
void SetSkill(int idx, const char* name, int cost, int dmg, int block, int heal, int unlocked,
    const char* dec, EffectType eff, int effVal)
{
    codexSkills[idx].id = idx;
    strcpy_s(codexSkills[idx].name, sizeof(codexSkills[idx].name), name);
    strcpy_s(codexSkills[idx].dec, sizeof(codexSkills[idx].dec), dec);
    codexSkills[idx].cost = cost;
    codexSkills[idx].dmg = dmg;
    codexSkills[idx].block = block;
    codexSkills[idx].heal = heal;
    codexSkills[idx].unlocked = unlocked;
    codexSkills[idx].eff = eff;
    codexSkills[idx].effVal = effVal;
}

void ApplyTurnEnd_Player(Player* p)
{
    if (p->poison > 0) {
        p->poison--;
        if (p->poison < 0) p->poison = 0;
    }

    if (p->burn > 0) {
        p->HP -= p->burn;
        if (p->HP < 0) p->HP = 0;
        p->burn--;
    }
    if (p->vuln > 0) p->vuln--;
    if (p->weak > 0) p->weak--;
}

void ApplyTurnEnd_Enemy(Enemy* e)
{
    if (e->burn > 0) {
        e->hp -= e->burn;
        if (e->hp < 0) e->hp = 0;
        e->burn--;
    }
    if (e->vuln > 0) e->vuln--;
    if (e->weak > 0) e->weak--;

    if (e->poison > 0) {
        e->poison--;
    }
}

void InitCodex()
{
    for (int i = 0; i < MAX_SKILLS; i++) {
        SetSkill(i, "", 99, 0, 0, 0, 0, "아직 없음", EFF_NONE, 0);
    }

    // ====== 층1 스킬 18개 (0~17) ======
    SetSkill(0, "휘두르기", 1, 10, 0, 0, 1, "기본적인 공격", EFF_NONE, 0);
    SetSkill(1, "방패들기", 1, 0, 6, 0, 1, "기본적인 방어", EFF_NONE, 0);
    SetSkill(2, "회복", 2, 0, 0, 10, 1, "기본적인 회복", EFF_NONE, 0);

    SetSkill(3, "속공", 0, 6, 0, 0, 0, "빠르고 간결한 공격", EFF_NONE, 0);
    SetSkill(4, "찌르기", 1, 7, 0, 0, 0, "독 상태 적에게 추가 피해 + 4", EFF_NONE, 0);
    SetSkill(5, "강타", 2, 12, 0, 0, 0, "취약 적에게 추가 피해 + 10", EFF_NONE, 0);

    SetSkill(6, "독 바르기", 1, 4, 0, 0, 0, "독 3 부여, 칼날에 독을 바르는 것 만큼 간단한 일은 없지", EFF_POISON, 3);
    SetSkill(7, "침식", 1, 0, 0, 0, 0, "독 5 부여", EFF_POISON, 5);
    SetSkill(8, "신경 독소", 2, 6, 0, 0, 0, "독 4 부여", EFF_POISON, 4);

    SetSkill(9, "화염 참격", 2, 14, 0, 0, 0, "화상 4 부여", EFF_BURN, 4);
    SetSkill(10, "불씨", 1, 4, 0, 0, 0, "화상 4 부여", EFF_BURN, 4);
    SetSkill(11, "잔불", 0, 0, 0, 0, 0, "0비용 화상 1", EFF_BURN, 1);

    SetSkill(12, "약점 노출", 1, 0, 0, 0, 0, "취약 2턴", EFF_VULN, 2);
    SetSkill(13, "기력 붕괴", 1, 6, 0, 0, 0, "약화 2턴", EFF_WEAK, 2);
    SetSkill(14, "압박", 2, 0, 0, 0, 0, "취약 3턴", EFF_VULN, 3);

    SetSkill(15, "집중", 2, 0, 17, 0, 0, "안정적인 방어", EFF_NONE, 0);
    SetSkill(16, "철벽", 2, 8, 16, 0, 0, "최선의 방어는 공격", EFF_NONE, 0);
    SetSkill(17, "흡혈", 2, 8, 0, 15, 0, "공격 후 회복", EFF_NONE, 0);

    // ====== 2층 스킬 18개 (18~35) ======

// 기본 상위 밸류(2층 표준 스탯)
    SetSkill(18, "안면 강타", 1, 15, 0, 0, 0, "안정적인 중급 공격", EFF_NONE, 0);
    SetSkill(19, "강화 방어", 1, 0, 11, 0, 0, "중급 방어", EFF_NONE, 0);
    SetSkill(20, "응급 처치", 1, 0, 0, 14, 0, "중급 회복", EFF_NONE, 0);

    // 0코(밸류 상승은 있지만 패시브(0코 2회)랑 같이 쓰면 강해짐)
    SetSkill(21, "속공+", 0, 8, 0, 0, 0, "0비용 공격", EFF_NONE, 0);
    SetSkill(22, "맹독은잔불속에", 0, 0, 0, 0, 0, "화상 2와 독 1를 부여한다", EFF_NONE, 0);

    // 독 라인 (1층보다 부여량/딜이 증가)
    SetSkill(23, "독칼", 1, 15, 0, 0, 0, "공격 + 독 2", EFF_POISON, 2);
    SetSkill(24, "그혈관속맹독을", 1, 5, 0, 0, 0, "독 6 부여", EFF_POISON, 7);
    SetSkill(25, "부식은상처를타고", 2, 20, 0, 0, 0, "독 상태면 추가 피해 + 8", EFF_NONE, 0);

    // 불 라인
    SetSkill(26, "화염 베기", 2, 20, 0, 0, 0, "공격 + 화상 3", EFF_BURN, 3);
    SetSkill(27, "방화", 1, 0, 0, 0, 0, "화상 6 부여", EFF_BURN, 6);
    SetSkill(28, "화상은피부를타고", 2, 16, 0, 0, 0, "화상 상태면 추가 피해 + 8", EFF_NONE, 0);

    // 취약/약화 라인 (2층부터 약화 효율을 더 올려줌)
    SetSkill(29, "균열", 1, 6, 0, 0, 0, "공격 + 취약 2", EFF_VULN, 2);
    SetSkill(30, "살갓 노출", 2, 0, 0, 0, 0, "취약 4턴, 철갑속 살갓을 노리는것만큼 반가운일은 없지", EFF_VULN, 5);

    SetSkill(31, "무력화", 1, 0, 0, 0, 0, "약화 4턴 + 취약 1턴", EFF_WEAK, 4);
    SetSkill(32, "기력 절단", 2, 10, 0, 0, 0, "공격 + 약화 2, 공격은 적한테 피해만 주는게 아니다", EFF_WEAK, 2);

    // 방어/공방 카드(2층은 여기서 안정감 생김)
    SetSkill(33, "수비 태세", 2, 0, 22, 0, 0, "높은 방어는 안정감을 높인다", EFF_NONE, 0);
    SetSkill(34, "방패 강타", 1, 10, 8, 0, 0, "방패는 막으라고만 있는게 아니다", EFF_NONE, 0);
    SetSkill(35, "흡혈 베기", 2, 12, 0, 16, 0, "공격 후 큰 회복", EFF_NONE, 0);

    // ====== 3층 스킬 18개 (36~53) ======

// 3층 기본 상위 밸류(표준)
// (2층보다 체감 확실히 ↑)
    SetSkill(36, "참격", 1, 18, 0, 0, 0, "높은 공격", EFF_NONE, 0);
    SetSkill(37, "철갑 방어", 1, 0, 14, 0, 0, "높은 방어", EFF_NONE, 0);
    SetSkill(38, "붕대 감기", 2, 0, 0, 25, 0, "높은 회복", EFF_NONE, 0);


    // 0코는 패시브(0코 2회) 때문에
    SetSkill(39, "속공++", 0, 10, 0, 0, 0, "0비용 공격(상급)", EFF_NONE, 0);
    SetSkill(40, "빈틈 찌르기", 0, 0, 0, 0, 0, "취약 1과 약화 1 부여한다", EFF_NONE, 0);


    // 41: 독+공격, 42: 대량 독, 43: 독 ‘폭발(추가피해)’용
    SetSkill(41, "맹독 절개", 1, 16, 0, 0, 0, "공격 + 독 3", EFF_POISON, 3);
    SetSkill(42, "치명독 주입", 2, 0, 0, 0, 0, "독 10 부여", EFF_POISON, 10);
    SetSkill(43, "독폭발", 2, 22, 0, 0, 0, "독 상태면 추가 피해 +12", EFF_NONE, 0);


    // 44: 불+공격, 45: 대량 불, 46: 불 연계 추가피해
    SetSkill(44, "지옥 베기", 2, 26, 0, 0, 0, "공격 + 화상 3", EFF_BURN, 3);
    SetSkill(45, "방화광", 2, 0, 0, 0, 0, "화상 10 부여", EFF_BURN, 10);
    SetSkill(46, "재가 되는 상처", 2, 20, 0, 0, 0, "화상 상태면 추가 피해 +12", EFF_NONE, 0);

    // 취약/약화 컨트롤(3층은 ‘약화로 생존’, ‘취약으로 마무리’)
    SetSkill(47, "신경 절단", 1, 10, 0, 0, 0, "공격 + 약화 3", EFF_WEAK, 3);
    SetSkill(48, "붕괴", 2, 0, 0, 0, 0, "취약 4턴", EFF_VULN, 4);
    SetSkill(49, "완전 무력화", 2, 0, 0, 0, 0, "약화 4턴", EFF_WEAK, 4);


    SetSkill(50, "강철 반격", 2, 18, 18, 0, 0, "공격과 방어를 동시에", EFF_NONE, 0);
    SetSkill(51, "피의 갈증", 2, 15, 0, 26, 0, "공격 후 대회복", EFF_NONE, 0);


    // 52: 취약이면 크게 강해짐, 53: 독/불 둘 중 하나라도 있으면 강해짐
    SetSkill(52, "처형", 2, 28, 0, 0, 0, "취약 상태면 추가 피해 +14", EFF_NONE, 0);
    SetSkill(53, "종결의 일격", 4, 30, 0, 0, 0, "독/화상 중 하나라도 있으면 추가 피해 +16", EFF_NONE, 0);

}

// --------------------
// 해금(상자/상인/처치) 유틸
// --------------------
int RandomLockedSkillOnFloor(int floor)
{
    int start = floor * SKILLS_PER_FLOOR;
    int tries = 200;

    while (tries--) {
        int idx = start + (rand() % SKILLS_PER_FLOOR);
        if (idx >= 0 && idx < MAX_SKILLS && codexSkills[idx].unlocked == 0)
            return idx;
    }
    return -1;
}

void UnlockOneSkill(Player* p, int floor)
{
    int idx = RandomLockedSkillOnFloor(floor);
    if (idx == -1) {
        renderMessage("이 층의 스킬은 이미 전부 해금됨. 아무 키.");
        wait_any_key();
        return;
    }

    codexSkills[idx].unlocked = 1;
    char buf[160];
    sprintf_s(buf, sizeof(buf),
        "스킬 해금: %s (도감에 기록됨) [E]=도감 열기 / 아무키=계속",
        codexSkills[idx].name);

    renderMessage(buf);
    wait_any_key();
}

// --------------------
// 일지
// --------------------
void DrawJournal(int floor, int jIndex)
{
    ClearScreenNoFlicker();
    drawbox(20, 5, 120, 36, " 일지 ");

    int x = 24;
    int y = 9;

    for (int i = 0; i < 40; i++) {
        const char* line = JOURNAL_TEXT[floor][jIndex][i];
        if (!line) break;
        gotoxy(x, y + i);
        printf("%-100s", line);
    }


    gotoxy(24, 38);
    printf("아무 키: 닫기");
    wait_any_key();
}

void HandleJournalEvent(Player* p, int floor, int jIndex)
{
    if (p->journalMask[floor] & (1 << jIndex)) {
        renderMessage("이미 읽은 일지다.");
        wait_any_key();
        return;
    }

    p->journalMask[floor] |= (1 << jIndex);
    p->gold += 20;
    char buf[128];
    sprintf_s(buf, sizeof(buf), "일지와 함께 소량의 골드를 발견했다! 골드 +20. 아무 키.");
    renderMessage(buf);
    wait_any_key();


    DrawJournal(floor, jIndex);
}

int CountReadJournals(Player* p, int floor)
{
    int cnt = 0;
    for (int i = 0; i < 3; i++) {
        if (p->journalMask[floor] & (1u << i)) cnt++;
    }
    return cnt;
}

// --------------------
// 제단
// --------------------
void AltarEvent(Player* p)
{
    DB_BeginFrame();

    drawbox(10, 5, 140, 30, " 제단 ");

    gotoxy(14, 8);
    printf("붉은 제단이 네게 네 가지 선택을 제시한다.");

    gotoxy(14, 10);
    printf("1) 피의 계약 : 현재 체력 -30 → 이 층 스킬 2개 해금");

    gotoxy(14, 12);
    printf("2) 금지된 지식 : 골드 60 소모 → 다음 층 스킬 1개 해금");

    gotoxy(14, 14);
    printf("3) 저주받은 축복 : 약화 3턴 → 스킬 1개 해금 + 방어막 12");

    gotoxy(14, 16);
    printf("4) 운명의 도박 : 50%% 성공(스킬 2개) / 50%% 실패(체력-20, 약화2)");

    gotoxy(14, 18);
    printf("0) 아무것도 하지 않는다");
    DB_Present();
    int ch = _getch();

    if (ch == '0' || ch == 27) {
        renderMessage("아무 일도 일어나지 않았다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '1') {
        int loss = 30;
        if (p->HP <= loss) loss = p->HP - 1;
        if (loss < 0) loss = 0;
        p->HP -= loss;

        UnlockOneSkill(p, p->floor);
        UnlockOneSkill(p, p->floor);

        renderMessage("피의 계약을 맺었다. 힘을 얻었지만 피를 흘렸다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '2') {
        if (p->gold < 60) {
            renderMessage("골드가 부족하다. (아무 키)");
            wait_any_key();
            return;
        }

        p->gold -= 60;

        int nextFloor = p->floor + 1;
        if (nextFloor >= FLOORS) nextFloor = FLOORS - 1;

        UnlockOneSkill(p, nextFloor);

        renderMessage("경험하지 못한 지식을 손에 넣었다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '3') {
        p->weak += 3;
        p->altarGuardBonus += 12;
        UnlockOneSkill(p, p->floor);

        renderMessage("저주와 축복이 동시에 내렸다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '4') {
        if ((rand() % 100) < 50) {
            UnlockOneSkill(p, p->floor);
            UnlockOneSkill(p, p->floor);
            renderMessage("대성공! 지식이 폭발적으로 열렸다! (아무 키)");
        }
        else {
            int loss = 50;
            if (p->HP <= loss) loss = p->HP - 1;
            if (loss < 0) loss = 0;
            p->HP -= loss;
            p->weak += 2;
            renderMessage("실패… 제단이 네 생명을 앗아갔다. (아무 키)");
        }
        wait_any_key();
        return;
    }
}

// --------------------
// 적 생성
// --------------------
Enemy makeEnemyForFloor(int floor)
{
    Enemy e;
    memset(&e, 0, sizeof(e));
    int type = rand() % 5;

    if (floor == 0) {
        const char* names[5] = { "슬라임", "박쥐", "고블린", "해골", "거미" };
        strcpy_s(e.name, sizeof(e.name), names[type]);
        e.maxHp = 35 + type * 5;
        e.atk = 6 + type;
        e.expReward = 6 + type;
    }
    else if (floor == 1) {
        const char* names[5] = { "강슬라임", "늑대", "망령", "전사해골", "독거미" };
        strcpy_s(e.name, sizeof(e.name), names[type]);
        e.maxHp = 55 + type * 7;
        e.atk = 9 + type * 2;
        e.expReward = 10 + type;
    }
    else {
        const char* names[5] = { "흑기사", "거대박쥐", "저주망령", "암흑전사", "독왕거미" };
        strcpy_s(e.name, sizeof(e.name), names[type]);
        e.maxHp = 80 + type * 10;
        e.atk = 13 + type * 3;
        e.expReward = 16 + type * 2;
    }

    e.hp = e.maxHp;
    e.intent = 0;
    e.kind = EN_NORMAL;
    return e;
}

Enemy makeEliteForFloor(int floor)
{
    Enemy e;
    memset(&e, 0, sizeof(e));

    int r = rand() % 3;

    if (floor == 0) {
        if (r == 0) { strcpy_s(e.name, sizeof(e.name), "파수병");   e.maxHp = 90;  e.atk = 10; e.expReward = 20; e.kind = EN_ELITE_GUARD; }
        else if (r == 1) { strcpy_s(e.name, sizeof(e.name), "사냥꾼"); e.maxHp = 75;  e.atk = 8;  e.expReward = 20; e.kind = EN_ELITE_HUNTER; }
        else { strcpy_s(e.name, sizeof(e.name), "부패한 기사");     e.maxHp = 85;  e.atk = 11; e.expReward = 22; e.kind = EN_ELITE_KNIGHT; }
    }
    else if (floor == 1) {
        if (r == 0) { strcpy_s(e.name, sizeof(e.name), "중갑 파수병"); e.maxHp = 130; e.atk = 14; e.expReward = 35; e.kind = EN_ELITE_GUARD; }
        else if (r == 1) { strcpy_s(e.name, sizeof(e.name), "늪의 사냥꾼"); e.maxHp = 115; e.atk = 12; e.expReward = 35; e.kind = EN_ELITE_HUNTER; }
        else { strcpy_s(e.name, sizeof(e.name), "타락 기사");        e.maxHp = 125; e.atk = 15; e.expReward = 38; e.kind = EN_ELITE_KNIGHT; }
    }
    else { // floor == 2 (3층)
        if (r == 0) { strcpy_s(e.name, sizeof(e.name), "철갑 파수병"); e.maxHp = 180; e.atk = 18; e.expReward = 55; e.kind = EN_ELITE_GUARD; }
        else if (r == 1) { strcpy_s(e.name, sizeof(e.name), "역병 사냥꾼"); e.maxHp = 160; e.atk = 16; e.expReward = 55; e.kind = EN_ELITE_HUNTER; }
        else { strcpy_s(e.name, sizeof(e.name), "심연 기사");         e.maxHp = 170; e.atk = 19; e.expReward = 60; e.kind = EN_ELITE_KNIGHT; }
    }

    e.hp = e.maxHp;
    return e;
}

Enemy makeBossForFloor(int floor)
{
    Enemy e; memset(&e, 0, sizeof(e));

    if (floor == 0) {
        strcpy_s(e.name, sizeof(e.name), "팔레르모");
        e.maxHp = 300;
        e.atk = 12;
        e.expReward = 40;
        e.kind = EN_BOSS_GATEKEEPER;
    }
    else if (floor == 1) {
        // 2층 보스: 타나토스 ((취약 연계)
        strcpy_s(e.name, sizeof(e.name), "타나토스");
        e.maxHp = 400;
        e.atk = 14;
        e.expReward = 70;
        e.kind = EN_BOSS_EXECUTIONER;
    }

    else if (floor == 2) {
        // 3층 보스: 팔랑크스 (독/화상)
        strcpy_s(e.name, sizeof(e.name), "팔랑크스");
        e.maxHp = 500;
        e.atk = 13;
        e.expReward = 90;
        e.kind = EN_BOSS_PLAGUE_CORE;
    }

    else if (floor == 3) {
        strcpy_s(e.name, sizeof(e.name), "???");  // 4층 보스 이름
        e.maxHp = 650;
        e.atk = 15;
        e.expReward = 0;
        e.kind = EN_BOSS_FINAL;
    }

    e.hp = e.maxHp;
    return e;
}


// --------------------
// 경험치/레벨업
// --------------------
void gainExp(Player* p, int amount)
{
    p->exp += amount;
    while (p->exp >= p->expToNext) {
        p->exp -= p->expToNext;
        p->level++;

        p->maxHP += 10;
        p->HP += 10;
        if (p->HP > p->maxHP) p->HP = p->maxHP;
        p->expToNext += 5;

        renderMessage("레벨업! 최대 체력 +10. 아무 키.");
        wait_any_key();
    }
}

// --------------------
// 전투 UI (오른쪽에 장착 6개 스킬 리스트)
// --------------------
void drawSkillCardMini(int x, int y, const Skill* s, int selected, Player* p)
{
    gotoxy(x, y);     printf("+----------------------+");
    gotoxy(x, y + 1); printf("|                      |");
    gotoxy(x, y + 2); printf("|                      |");
    gotoxy(x, y + 3); printf("|                      |");
    gotoxy(x, y + 4); printf("+----------------------+");

    if (selected) {
        gotoxy(x - 2, y + 2); printf(">>");
        gotoxy(x + 24, y + 2); printf("<<");
    }

    if (!s) {
        gotoxy(x + 2, y + 1);
        printf("(비어있음)");
        return;
    }

    gotoxy(x + 2, y + 1);  printf("%-16s", s->name);
    gotoxy(x + 19, y + 1); printf("비용:%d", s->cost);
    gotoxy(x + 2, y + 2);  printf("피:%2d 방:%2d 회:%2d", s->dmg, s->block, s->heal);
    // 상태 요약 (카드 하단)
    gotoxy(x + 2, y + 3);

    // 기본은 공백으로 지워두기
    printf("%-20s", " ");

    gotoxy(x + 2, y + 3);
    switch (s->eff) {
    case EFF_POISON:
        printf("독:%d", s->effVal);
        break;
    case EFF_BURN:
        printf("화:%d", s->effVal);
        break;
    case EFF_VULN:
        printf("취:%d", s->effVal);
        break;
    case EFF_WEAK:
        printf("약:%d", s->effVal);
        break;
    default:
        // 효과 없는 스킬은 아무것도 표시 안 함
        break;
    }


}

void drawEquippedSkillsUI(Player* p, int selected)
{
    int baseX = CARD_UI_X;
    int baseY = CARD_UI_Y;
    int gapY = 5;

    for (int i = 0; i < MAX_EQUIP; i++) {
        Skill* s = NULL;
        if (p->equip[i] != -1) s = &codexSkills[p->equip[i]];
        drawSkillCardMini(baseX, baseY + i * gapY, s, (i == selected), p);

    }
}

// --------------------
// 전투 로직
// --------------------
int applySkill(Player* p, Enemy* e, int* block, int* energy, Skill* s)
{
    if (!s) {
        renderMessage("비어있는 슬롯이다. 아무 키.");
        return 0;
    }
    if (*energy < s->cost) {
        renderMessage("에너지가 부족하다. 아무 키.");
        return 0;
    }

    *energy -= s->cost;

    // --- 상태이상 보너스(패시브) ---
    int bonus = PassiveStatusBonus(p);

    // --- 공격 처리 ---
    if (s->dmg > 0) {
        
        int base = s->dmg;
       
        
       
        // 추가피해(연계) 누적
        int bonusDmg = 0;

        // 1층
        if (s->id == 4 && e->poison > 0) bonusDmg += 4;   // 찌르기
        if (s->id == 5 && e->vuln > 0) bonusDmg += 10;  // 강타

        // 2층
        if (s->id == 25 && e->poison > 0) bonusDmg += 8;  // 부식은 상처를 타고
        if (s->id == 28 && e->burn > 0) bonusDmg += 8;  // 화상은 피부를 타고

        // 3층
        if (s->id == 43 && e->poison > 0) bonusDmg += 12; // 독폭발
        if (s->id == 46 && e->burn > 0) bonusDmg += 12; // 재가 되는 상처
        if (s->id == 52 && e->vuln > 0) bonusDmg += 14; // 처형
        if (s->id == 53 && (e->poison > 0 || e->burn > 0)) bonusDmg += 16; // 종결의 일격

        base += bonusDmg;



        int real = CalcPlayerDamage(base, e, p);
        e->hp -= real;
        if (e->hp < 0) e->hp = 0;


        if (e->poison > 0) {
            e->poison--;
            if (e->poison < 0) e->poison = 0;
        }
    }

    // --- 특수카드: 22번(독+불 복합) ---
    if (s->id == 22) {

        e->burn += (2 + bonus);
        e->poison += (1 + bonus);

        if (e->burn > 30)   e->burn = 30;
        if (e->poison > 30) e->poison = 30;
    }

    // --- 특수카드: 31번(약화 + 취약1 추가) ---
    if (s->id == 31) {
        e->vuln += 1;

    }
    // --- 특수카드 : 40
    if (s->id == 40) {
        e->vuln += 1;
        e->weak += 1;
    }
    // --- 방어/회복 ---
    if (s->block > 0) *block += s->block;

    if (s->heal > 0) {
        p->HP += s->heal;
        if (p->HP > p->maxHP) p->HP = p->maxHP;
    }

    // --- 일반 효과 적용 ---
    int effVal = s->effVal;

    if (s->eff == EFF_POISON && e->kind == EN_ELITE_HUNTER) {
        effVal = effVal / 2; // 독 50% 감소
    }

    switch (s->eff) {
    case EFF_POISON:
        e->poison += (effVal + bonus);
        if (e->poison > 30) e->poison = 30;
        break;

    case EFF_BURN:
        e->burn += (effVal + bonus);
        if (e->burn > 30) e->burn = 30;
        break;

    case EFF_VULN:
        e->vuln += effVal;
        break;

    case EFF_WEAK:
        e->weak += effVal;
        break;

    default:
        break;
    }

    char buf[160];
    sprintf_s(buf, sizeof(buf), "사용: %s | 남은에너지:%d | 적체력:%d/%d (아무 키)",
        s->name, *energy, e->hp, e->maxHp);
    renderMessage(buf);
    wait_any_key();

    return 1;
}

void enemyTurn(Player* p, Enemy* e, int* block)
{
    if (e->kind == EN_BOSS_FINAL) {
        int roll = rand() % 100;

        if (roll < 40) {
            int dmg = 42;
            if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
            int taken = dmg;

            if (*block > 0) {
                int used = (*block >= taken) ? taken : *block;
                *block -= used;
                taken -= used;
            }
            if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }

            renderMessage("???의 %$#%을 마주한다. (아무 키)");
            wait_any_key();
            return;
        }
        else if (roll < 60) {
            p->vuln += 4;
            renderMessage("???의 시선이 꿰뚫는다. 취약 4. (아무 키)");
            wait_any_key();
            return;
        }
        else {
            for (int hit = 0; hit < 2; hit++) {
                int dmg = 20;
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;

                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }
            }
            renderMessage("???의 ?%&@을 목도한다. (아무 키)");
            wait_any_key();
            return;
        }
    }


    // 팔레르모
    if (e->kind == EN_BOSS_GATEKEEPER) {
        int phase2 = (e->hp <= e->maxHp / 2);

        int roll = rand() % 100;

        if (!phase2) {
            // 페이즈1: 60% 기본, 30% 강공, 10% 효과
            if (roll < 10) {
                p->vuln += 2;
                renderMessage("팔레르모의 '유리한 결투' ! 일대를 약화시켰다. (아무 키)");
                wait_any_key();

                return;
            }
            else if (roll < 40) {
                // 강공 18
                int dmg = 25;
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;

                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) {
                    p->HP -= taken;
                    if (p->HP < 0) p->HP = 0;
                }
                renderMessage("팔레르모의 '카치아토레' ! (아무 키)");
                wait_any_key();
                return;
            }
            else {
                // 기본 15
                int dmg = 20;
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;

                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) {
                    p->HP -= taken;
                    if (p->HP < 0) p->HP = 0;
                }
                renderMessage("팔레르모의 '스파다' ! (아무 키)");
                wait_any_key();
                return;
            }
        }
        else {
            // 페이즈2: 70% 강공 20, 30% 연속 8+8
            if (roll < 30) {
                // 연속 8 + 8
                for (int hit = 0; hit < 2; hit++) {
                    int dmg = 11;
                    if (e->weak > 0) dmg = dmg * 75 / 100;   // 원래도 e->weak (유지)
                    int taken = dmg;
                    if (*block > 0) {
                        int used = (*block >= taken) ? taken : *block;
                        *block -= used;
                        taken -= used;
                    }
                    if (taken > 0) {
                        p->HP -= taken;
                        if (p->HP < 0) p->HP = 0;
                    }
                }
                renderMessage("팔레르모의 '세치오나투라' ! (아무 키)");
                wait_any_key();
                return;
            }
            else {
                int dmg = 23;
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;

                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) {
                    p->HP -= taken;
                    if (p->HP < 0) p->HP = 0;
                }
                renderMessage("팔레르모의 '디 체르보' ! (아무 키)");
                wait_any_key();
                return;
            }
        }
    }

    // 타나토스
    if (e->kind == EN_BOSS_EXECUTIONER) {
        int phase2 = (e->hp <= e->maxHp / 2);
        int roll = rand() % 100;

        // 1) 취약 부여 + 약한 공격
        if (!phase2) {
            if (roll < 35) {
                int dmg = 20;
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;

                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }

                p->vuln += 2;   // 플레이어 취약 2
                renderMessage("타나토스가 약점을 노린다! 취약 2. (아무 키)");
                wait_any_key();
                return;
            }

            // 2) 기본 공격
            if (roll < 75) {
                int dmg = 26;
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;

                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }

                renderMessage("타나토스의 '장대비 베기' ! (아무 키)");
                wait_any_key();
                return;
            }

            // 3) 공격(취약이면 추가 피해)
            {
                int dmg = 18;
                if (p->vuln > 0) dmg += 14; // 플레이어 취약 상태면 더 아픔 (이건 p->vuln 맞음)
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;
                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }

                renderMessage("타나토스의 '메기도라' ! 취약시 추가 피해 (아무 키)");
                wait_any_key();
                return;
            }
        }
        else {
            // phase2: 더 공격적
            if (roll < 30) {
                // 연속타 2회
                for (int hit = 0; hit < 2; hit++) {
                    int dmg = 13;
                    if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                    int taken = dmg;
                    if (*block > 0) {
                        int used = (*block >= taken) ? taken : *block;
                        *block -= used;
                        taken -= used;
                    }
                    if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }
                }
                p->vuln += 1;
                renderMessage("타나토스의 '망자의 탄식' ! 취약 1. (아무 키)");
                wait_any_key();
                return;
            }

            // phase2 빈도 증가
            {
                int dmg = 20;
                if (p->vuln > 0) dmg += 18;   // 플레이어 취약시 추가피해 (p->vuln 맞음)
                if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
                int taken = dmg;
                if (*block > 0) {
                    int used = (*block >= taken) ? taken : *block;
                    *block -= used;
                    taken -= used;
                }
                if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }

                renderMessage("타나토스의'브레이브 재퍼'! (아무 키)");
                wait_any_key();
                return;
            }
        }
    }

    // 3층 보스: 팔랑크스
    if (e->kind == EN_BOSS_PLAGUE_CORE) {
        int phase2 = (e->hp <= e->maxHp / 2);
        int roll = rand() % 100;

        // 1) 역병 확산: 플레이어에게 독/화상 누적
        if (roll < (phase2 ? 25 : 10)) {
            int addP = phase2 ? 3 : 2;
            int addB = phase2 ? 3 : 2;

            p->poison += addP;
            p->burn += addB;
            if (p->poison > 30) p->poison = 30;
            if (p->burn > 30) p->burn = 30;

            renderMessage("팔랑크스가 독과 불을 퍼뜨린다 ! (아무 키)");
            wait_any_key();
            return;
        }

        // 2) 기본 공격 (플레이어 상태이상 있으면 추가)
        if (roll < (phase2 ? 80 : 60)) {
            int dmg = 28;
            if (p->poison > 0) dmg += 4;   // 플레이어가 독 상태면 추가피해 (p->poison 맞음)
            if (p->burn > 0)   dmg += 4;   // 플레이어가 화상 상태면 추가피해 (p->burn 맞음)
            if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
            int taken = dmg;
            if (*block > 0) {
                int used = (*block >= taken) ? taken : *block;
                *block -= used;
                taken -= used;
            }
            if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }

            renderMessage("팔랑크스의 '도리' ! 상태이상시 추가피해 (아무 키)");
            wait_any_key();
            return;
        }

        // 3) 플레이어 상태이상 기반 큰 피해
        {
            int dmg = 18 + (p->poison / 2) + (p->burn / 2);
            if (phase2) dmg += 10;
            if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
            int taken = dmg;
            if (*block > 0) {
                int used = (*block >= taken) ? taken : *block;
                *block -= used;
                taken -= used;
            }
            if (taken > 0) { p->HP -= taken; if (p->HP < 0) p->HP = 0; }

            if (p->poison > 0) p->poison = (p->poison > 4) ? (p->poison - 4) : 0;
            if (p->burn > 0)   p->burn = (p->burn > 4) ? (p->burn - 4) : 0;

            renderMessage("팔랑크스의 '호플리타이' ! 상태이상이 터진다. (아무 키)");
            wait_any_key();
            return;
        }
    }

    // ===== 강적: 늪사냥꾼(플레이어 독 부여) =====
    if (e->kind == EN_ELITE_HUNTER) {

        int roll = rand() % 100;

        if (roll < 60) {
            // 패턴 A: 기본공격 + 독2
            int dmg = e->atk;
            if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak

            int taken = dmg;
            if (*block > 0) {
                int used = (*block >= taken) ? taken : *block;
                *block -= used;
                taken -= used;
            }
            if (taken > 0) {
                p->HP -= taken;
                if (p->HP < 0) p->HP = 0;
            }

            p->poison += 2;
            if (p->poison > 30) p->poison = 30;

            renderMessage("늪사냥꾼의 공격! 독 2가 스민다. (아무 키)");
            wait_any_key();
            return;
        }
        else if (roll < 85) {
            // 패턴 B: 강공(독 없음)
            int dmg = e->atk + 8;
            if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak

            int taken = dmg;
            if (*block > 0) {
                int used = (*block >= taken) ? taken : *block;
                *block -= used;
                taken -= used;
            }
            if (taken > 0) {
                p->HP -= taken;
                if (p->HP < 0) p->HP = 0;
            }

            renderMessage("늪사냥꾼의 강공! (아무 키)");
            wait_any_key();
            return;
        }
        else {
            // 패턴 C
            p->weak += 2;   // 플레이어 약화 1
            if (p->weak > 30) p->weak = 30;

            renderMessage("늪사냥꾼의 함정! 약화 1 (아무 키)");
            wait_any_key();
            return;
        }
    }


    // ===== 강적: 부패한 기사(방어 관통 + 취약) =====
    if (e->kind == EN_ELITE_KNIGHT) {
        int roll = rand() % 100;
        int dmg = (roll < 30) ? 14 : 11; // 특수 30%
        if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak

        int piercePct = 30; // 방어 관통 30%

        // 관통 피해(방어 무시)
        int bypass = dmg * piercePct / 100;
        int rest = dmg - bypass;

        // rest는 방어로 막을 수 있음
        int taken = rest;
        if (*block > 0) {
            int used = (*block >= taken) ? taken : *block;
            *block -= used;
            taken -= used;
        }

        int totalTaken = bypass + taken;
        if (totalTaken > 0) {
            p->HP -= totalTaken;
            if (p->HP < 0) p->HP = 0;
        }

        if (roll < 30) p->vuln += 1; // 특수면 취약1

        renderMessage((roll < 30) ? "부패한 기사의 특수 공격! 취약 1 (아무 키)"
            : "부패한 기사의 관통 공격! (아무 키)");
        wait_any_key();
        return;
    }

    // ===== 강적: 파수병(공격/강공 위주) =====
    if (e->kind == EN_ELITE_GUARD) {
        int roll = rand() % 100;
        int dmg = (roll < 30) ? (e->atk + 6) : e->atk; // 30% 강공
        if (e->weak > 0) dmg = dmg * 75 / 100;   // ← 수정: e->weak
        int taken = dmg;

        if (*block > 0) {
            int used = (*block >= taken) ? taken : *block;
            *block -= used;
            taken -= used;
        }
        if (taken > 0) {
            p->HP -= taken;
            if (p->HP < 0) p->HP = 0;
        }

        renderMessage((roll < 30) ? "파수병의 강공! (아무 키)" : "파수병의 공격! (아무 키)");
        wait_any_key();
        return;
    }

    // ===== 일반몹(기존 로직) =====
    int roll = rand() % 100;

    if (roll < 50) {
        e->intent = 0;   // 공격 50%
    }
    else if (roll < 85) {
        e->intent = 1;   // 강공 35%
    }
    else {
        e->intent = 2;   // 방어 15%
    }


    if (e->intent == 2) {
        renderMessage("적이 숨을 고른다. 아무 키.");
        wait_any_key();
        return;
    }

    int dmg = e->atk + (e->intent == 1 ? 5 : 0);

    if (e->weak > 0) {                  // ← 수정: e->weak
        dmg = dmg * 75 / 100;
    }
    int taken = dmg;

    if (*block > 0) {
        int used = (*block >= taken) ? taken : *block;
        *block -= used;
        taken -= used;
    }

    if (taken > 0) {
        p->HP -= taken;
        if (p->HP < 0) p->HP = 0;
    }

    char buf[128];
    if (e->intent == 1)
        sprintf_s(buf, sizeof(buf), "%s의 강공! 피해:%d (아무 키)", e->name, dmg);
    else
        sprintf_s(buf, sizeof(buf), "%s의 공격! 피해:%d (아무 키)", e->name, dmg);

    renderMessage(buf);
    wait_any_key();
}

void RespawnAtCheckpoint(Player* p)
{
    if (p->hasCheckpoint) {
        p->x = p->cpX;
        p->y = p->cpY;
    }
    else {
        p->x = 1;
        p->y = 1;
    }
    p->HP = p->maxHP;
}

void renderMaze(Player* p, char map[H][W + 1])
{
    int ox = M_X + 2;
    int oy = M_Y + 2;

    for (int y = 0; y < H; y++) {
        gotoxy(ox, oy + y);
        for (int x = 0; x < W; x++) {

            // 미탐색이면 그냥 비움
            if (!p->seen[p->floor][y][x]) {
                printf("  ");
                continue;
            }

            // 플레이어는 항상 표시
            if (p->x == x && p->y == y) {
                printf("%c ", dirchar(p->dir));
                continue;
            }

            if (map[y][x] == '#') {
                printf("##");
            }
            else {
                char t = map[y][x];
                if (t == 'B' || t == 'C' || t == 'S' || t == 'E' || t == 'U' || t == 'I' || t == '!' || t == '@')
                    printf("%c ", t);
                else
                    printf("  ");
            }
        }
    }
}
void DrawTopInfo(Player* p)
{
    gotoxy(42, 3);
    printf("%d층   ", p->floor + 1);

    gotoxy(47, 3);
    printf("%-20s", "");
    gotoxy(47, 3);
    printf("체크포인트 %c", p->hasCheckpoint ? 'O' : 'X');
}
void narrationPhal(Player*p, char map[H][W + 1])
{
    DB_BeginFrame();

    NarrHeader();
    printf("                                                 [Enter] 다음\n\n");
    DB_Present();
    waitenter();
    printf("                                                「끝났네…」 \n\n");
    DB_Present();
    waitenter();
    printf("                                                 기나긴 혈투 끝에 팔랑크스는 무너졌다\n\n");
    DB_Present();
    waitenter();
    printf("                                                「아니, 끝난 건 아니지」 \n\n");
    DB_Present();
    waitenter();
    printf("                                                 당신은 심장을 겨낭한후 칼을 꼿아 넣는다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                「...뭐야..?」 \n\n");
    DB_Present();
    waitenter();
    printf("                                                 그러나 느껴지는건 없다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 당신이 잡은것은 그저 '껍데기' 일 뿐이다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 마치 곤충의 껍질같은 지금의 팔랑크스는 그저 껍질이다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 던전에서 나갈수있는 귀환석은 보이지 않는다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 던전이 떨리며 문이 열린다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 만약 나아간다면 두 번 다신 이곳에 올 수 없을것이다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 만일을 위해 이 층에서 준비를 하는 것이 현명하다.\n\n");
    DB_Present();
    waitenter();
    DB_BeginFrame();
    drawUIFrame();
    playerstate(p);
    DrawPassiveUI(p);
    renderSkillBookObject();
    renderMaze(p, map);
    DrawTopInfo(p);
    gotoxy(42, 22);
    printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
    renderMessage(pickRandomLine());
    DB_Present();  // ← 이 줄 추가!
}
void narrationp_end(Player* p, char map[H][W + 1])
{
    DB_BeginFrame();
    NarrHeader();
    printf("                                                 [Enter] 다음\n\n");
    DB_Present();
    waitenter();
    printf("                                                「죽였다」 \n\n");
    DB_Present();
    waitenter();
    printf("                                                 알수없는 생명체는 눈 앞에 죽어있다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 그렇지만 귀환석은 보이지않는다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                「왜...안..보이건데..젠장!」\n\n");
    DB_Present();
    waitenter();
    printf("                                                 던전은 다시 움직인다. \n\n");
    DB_Present();
    waitenter();
    printf("                                                 문이열린다.\n\n");
    DB_Present();
    waitenter();
    printf("                                                 나아간다 다시.\n\n");
    DB_Present();
    waitenter();
   
    DB_Present();
    waitenter();
    DB_BeginFrame();
    drawUIFrame();
    playerstate(p);
    DrawPassiveUI(p);
    renderSkillBookObject();
    renderMaze(p, map);
    DrawTopInfo(p);
    gotoxy(42, 22);
    printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
    renderMessage(pickRandomLine());
    DB_Present();  // ← 이 줄 추가!
}

int battleLoop(Player* p, EncounterType type, char map[H][W + 1])
{
    Enemy enemy;
    memset(&enemy, 0, sizeof(enemy));

    if (type == ENC_ELITE) enemy = makeEliteForFloor(p->floor);
    else if (type == ENC_BOSS) enemy = makeBossForFloor(p->floor);
    else enemy = makeEnemyForFloor(p->floor);
    int canEscape = (type != ENC_BOSS);
    int energyMax = PlayerEnergyMax(p);
    int energy = energyMax;
    if (HasPassive(p, PASSIVE_FINAL_PREP)) {
        energy += 2;
    }
    int block = 0;
    int selected = 0;

    int zeroUsedThisTurn = 0;
    int zeroLimit = PassiveZeroCostLimit(p);

    block += PassiveBattleStartBlock(p);

    if (p->altarGuardBonus > 0) {
        block += p->altarGuardBonus;
        p->altarGuardBonus = 0;
    }

    if (enemy.kind == EN_ELITE_GUARD) block += 12;

    while (1) {
        if (enemy.hp <= 0) {
            p->gritUsed = 0;
            int g = 10 + rand() % 8;
            int bonusG = PassiveKillGoldBonus(p);
            p->gold += (g + bonusG);

            char buf[128];
            sprintf_s(buf, sizeof(buf), "승리! 골드 %d(+%d). 아무 키.", g, bonusG);
            renderMessage(buf);
            wait_any_key();

            gainExp(p, enemy.expReward);

            if ((rand() % 100) < 35) {
                renderMessage("보상 발견! 스킬 1개 해금. 아무 키.");
                wait_any_key();
                UnlockOneSkill(p, p->floor);
            }
            if (enemy.kind == EN_BOSS_PLAGUE_CORE) {
                narrationPhal(p,map);
              
            }
            if (enemy.kind == EN_BOSS_FINAL)
            {
                narrationp_end(p,map);
            }
            return 1;
        }

        if (p->HP <= 0) {
          
            if (HasPassive(p, PASSIVE_GRIT_HEART) && p->gritUsed == 0) {
                p->gritUsed = 1;
                p->HP = p->maxHP * 30 / 100;
                renderMessage("집념의 심장 발동! 체력 30%로 부활! (아무 키)");
                wait_any_key();
            }
            else {
                // 기존 패배 처리
                renderMessage("패배... 체크포인트로 이동한다. 아무 키.");
                wait_any_key();
                RespawnAtCheckpoint(p);
                return 0;
            }
        }

        DB_BeginFrame();
        drawUIFrame();
        playerstate(p);
        DrawPassiveUI(p);
        renderSkillBookObject();

        clearBoxInner(M_X, M_Y, M_W, M_H);
        gotoxy(M_X + 2, M_Y + 2);  printf("<< 전투 >>");
        gotoxy(M_X + 2, M_Y + 4);  printf("적: %-10s 체력:%3d/%3d", enemy.name, enemy.hp, enemy.maxHp);
        gotoxy(M_X + 2, M_Y + 12);  printf("에너지:%d/%d 방어막:%d", energy, energyMax, block);
        gotoxy(M_X + 2, M_Y + 18); printf("조작: ↑↓ 선택 | ENTER 사용 | SPACE 턴종료 | ESC 도주");

        gotoxy(M_X + 2, M_Y + 8);
        printf("적 상태: ");
        if (enemy.poison > 0) printf("[독%d] ", enemy.poison);
        if (enemy.burn > 0)   printf("[화%d] ", enemy.burn);
        if (enemy.vuln > 0)   printf("[취%d] ", enemy.vuln);
        if (enemy.weak > 0)   printf("[약%d] ", enemy.weak);

        gotoxy(M_X + 2, M_Y + 10);
        printf("플레이어 상태: ");
        if (p->poison > 0) printf("[독%d] ", p->poison);
        if (p->burn > 0)   printf("[화%d] ", p->burn);
        if (p->vuln > 0)   printf("[취%d] ", p->vuln);
        if (p->weak > 0)   printf("[약%d] ", p->weak);
        drawEquippedSkillsUI(p, selected);
        DB_Present();

        int ch = _getch();

        if (ch == 27) { // ESC 도주
            if (!canEscape) {
                renderMessage("지금의 전투에서는 도망칠 수 없다. (아무 키)");
                wait_any_key();
                continue;
            }

            int roll = rand() % 100;
            if (roll < 50) {
                renderMessage("가까스로 전투에서 벗어났다. 아무 키.");
                wait_any_key();
                return 0;
            }
            else {
                int dmg = enemy.atk;
                renderMessage("도주 실패. 적의 반격을 받았다. 아무 키.");
                wait_any_key();

                int taken = dmg;
                if (block > 0) {
                    int used = (block >= taken) ? taken : block;
                    block -= used;
                    taken -= used;
                }
                if (taken > 0) {
                    p->HP -= taken;
                    if (p->HP < 0) p->HP = 0;
                }

                char buf[128];
                sprintf_s(buf, sizeof(buf),
                    "반격 피해 %d! (체력 %d/%d) 아무 키.",
                    dmg, p->HP, p->maxHP);
                renderMessage(buf);
                wait_any_key();

                ApplyTurnEnd_Player(p);
                ApplyTurnEnd_Enemy(&enemy);

                energy = energyMax;
                zeroUsedThisTurn = 0;
                block = 0;
                continue;
            }
        }

        else if (ch == ' ') {
            ApplyTurnEnd_Player(p);
            enemyTurn(p, &enemy, &block);
            ApplyTurnEnd_Enemy(&enemy);

            energy = energyMax;
            zeroUsedThisTurn = 0;
            block = 0;
            continue;
        }
        else if (ch == 13) {
            if (p->equip[selected] == -1) {
                renderMessage("이 슬롯은 비어있다. 도감(E)에서 장착해라. 아무 키.");
                wait_any_key();
            }
            else {
                Skill* s = &codexSkills[p->equip[selected]];
                if (s && s->cost == 0 && zeroUsedThisTurn >= zeroLimit) {
                    renderMessage("0비용 스킬 사용 한도 초과! (아무 키)");
                    wait_any_key();
                }
                else if (applySkill(p, &enemy, &block, &energy, s)) {
                    if (s && s->cost == 0) zeroUsedThisTurn++;
                }
            }
            continue;
        }
        else if (ch == 224 || ch == 0) {
            int k = _getch();
            if (k == 72) selected = (selected + MAX_EQUIP - 1) % MAX_EQUIP;
            else if (k == 80) selected = (selected + 1) % MAX_EQUIP;
            continue;
        }
    }
}

// --------------------
// 랜덤 문장
// --------------------


void renderAction(int ch)
{
    gotoxy(MSG_X + 2, MSG_Y + 4);
    if (ch == 'w' || ch == 'W') printf("%-66s", "앞으로 이동했다");
    else if (ch == 's' || ch == 'S') printf("%-66s", "뒤로 이동했다");
    else if (ch == 'a' || ch == 'A') printf("%-66s", "왼쪽을 바라본다");
    else if (ch == 'd' || ch == 'D') printf("%-66s", "오른쪽을 바라본다");
    else if (ch == 'f' || ch == 'F') printf("%-66s", "층을 이동한다");
    else if (ch == 'e' || ch == 'E') printf("%-66s", "도감을 연다");
    else printf("%-66s", "...");
}

// --------------------
// 이동/던전
// --------------------
void turnL(Player* p) { p->dir = (Dir)((p->dir + 3) % 4); }
void turnR(Player* p) { p->dir = (Dir)((p->dir + 1) % 4); }

void wfront(Dir d, int* fx, int* fy)
{
    *fx = 0; *fy = 0;
    if (d == NORTH) *fy = -1;
    else if (d == SOUTH) *fy = 1;
    else if (d == WEST) *fx = -1;
    else if (d == EAST) *fx = 1;
}

int movefront(Player* p, char map[H][W + 1])
{
    int fx, fy;
    wfront(p->dir, &fx, &fy);
    int nx = p->x + fx;
    int ny = p->y + fy;

    if (nx >= 0 && nx < W && ny >= 0 && ny < H && map[ny][nx] != '#') {
        p->x = nx;
        p->y = ny;
        return 1;
    }
    return 0;
}

int moveBack(Player* p, char map[H][W + 1])
{
    int fx, fy;
    wfront(p->dir, &fx, &fy);
    int nx = p->x - fx;
    int ny = p->y - fy;

    if (nx >= 0 && nx < W && ny >= 0 && ny < H && map[ny][nx] != '#') {
        p->x = nx;
        p->y = ny;
        return 1;
    }
    return 0;
}

int checkEncounterAfterSafeMoves(Player*p)
{
    if (p->floor == 3) return 0;
    if (p->floor == 4) return 0;

    moveCountSinceLastBattle++;
    if (moveCountSinceLastBattle <= 14) return 0;
    return (rand() % 100) < 30;
}

void shownarration_G()
{
    NarrHeader();
    printf("                                                「아....」");
    waitenter();
    printf("                                                「아....아아..」");
    waitenter();
    printf("                                                「아....아...하..」");
    waitenter();
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    printf("                                                 %$#$$!@#$#$@#$#$@");
    waitenter();
    printf("                                                「q」");
    waitenter();
    printf("                                                「어서」");
    waitenter();
    printf("                                                  어서$%%@#$$%@세요");
    waitenter();
    printf("                                                  q : 목숨을 끊는다");
    waitenter();
    printf("                                                     q : 목숨을 끊는다");
    waitenter();
    printf("                                                   q : 목숨을 끊는다");
    waitenter();
    printf("                                                  q : 목숨을 끊는다");
    waitenter();
    printf("                                                    q : 목숨을 끊는다");
    printf("                                                 q : 목숨을 끊는다");
    waitenter();
    printf("                                                    q : 목숨을 끊는다");
    waitenter();
    printf("                                                 q : 목숨을 끊는다");
    waitenter();
    printf("                                                  q : 목숨을 끊는다");
    waitenter();
    printf("                                                 q : 목숨을 끊는다");
}

void DoGEvent(Player* p)
{
    DB_BeginFrame();
    NarrHeader();
    printf("                                                 [Enter] 다음\n\n");
    waitenter();
    printf("                                                「보았다」 \n\n");
    waitenter();
    printf("                                                 당신은 ??? 보았다.\n\n");
    waitenter();
    printf("                                                 $#%$@#$@@$#$#$$#&&%\n\n");
    waitenter();
    printf("                                                 당신은 @&%???#$ 보았다.\n\n");
    waitenter();
    printf("                                                「왜...?..내가」\n\n");
    waitenter();
    printf("                                                 이유는 없다. 예정이기에 운명이기에 \n\n");
    waitenter();
    printf("                                                 당신은 #@%$%#@새#%신#$을보았을 뿐이다. \n\n");
    waitenter();
    printf("                                                「.....아」 \n\n");
    waitenter();
    printf("                                                 어서 목숨을 끊는다. \n\n");
    waitenter();
    printf("                                                 'Q' \n\n");
    waitenter();
    DB_Present();

    shownarration_G();
}


// 층별 맵
void LoadMapForFloor(int floor, char map[H][W + 1])
{
    // floor0
    const char* base1[H] = {
        "#################################",
        "#.#B..#U....#...................#",
        "#.#...#.......########!########.#",
        "#.###.#######.#####.............#",
        "#I............I..C#B............#",
        "######.####################..####",
        "#....#.#.........U#.............#",
        "#..B.#.#..........###.#.........#",
        "#....#.########.##..#.####!######",
        "#....................C#.........#",
        "#I#####.#.............#....######",
        "#.....#.###############...C######",
        "#....S#...........#B.........@.E#",
        "#################################"
    };
    // floor1
    const char* base2[H] = {
        "#################################",
        "#....#B.....#C..............#B.B#",
        "#.##.#.####.#.##########....#.#.#",
        "#.#..#...I#.#.......#.....#.#.#.#",
        "#.#.#######.#.###.#.####..#.#.#.#",
        "#.#C.......!#...#.#.....#.#.....#",
        "#.#######.###.#.#.#####.#.#####!#",
        "#.....U.......#.#I..............#",
        "###.###########.#####.#####.###.#",
        "#...#.....#.....#.......#C#.....#",
        "#.#.#.###.#.#####.#.#####.#.#.###",
        "#.#...#...#....S#.#..........@..#",
        "#B#####I#########.#.###S#####..E#",
        "#################################"
    };
    // floor2
    const char* base3[H] = {
        "#################################",
        "#............#..................#",
        "#....#.......#C............#....#",
        "#..#########.#######.#########..#",
        "#.B#....#...............#....#B.#",
        "####....#....#.....#....#....####",
        "#..#..#...##.#.#.#.#.##...#..#..#",
        "#.###.#####...........#####.###.#",
        "#.........#....S.C...I#.........#",
        "#..#.#!#..######.######..#!#.#..#",
        "#....#.#..#I...#@#...I#..#.#....#",
        "#....#.####....#.#....####.#....#",
        "#B...#.........#E#.........#...B#",
        "#################################"
    };
    const char* base4[H] = {
       "#################################",
       "#................################",
       "################.################",
       "################.################",
       "################.################",
       "################.################",
       "################I################",
       "################.################",
       "################.################",
       "###############S.C###############",
       "###############...###############",
       "################@################",
       "##########...#..E..#...##########",
       "#################################"
    };
    const char* base5[H] = {
       "#################################",
       "#...#@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#..G#@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#####@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#",
       "#################################"
    };
    const char** src = base1;
    if (floor == 1) src = base2;
    else if (floor == 2) src = base3;
    else if (floor == 3) src = base4;
    else if (floor == 4) src = base5;
    for (int y = 0; y < H; y++) {
        strcpy_s(map[y], W + 1, src[y]);
    }
}

// 체크포인트


void HandleTileEvent(Player* p, char map[H][W + 1])
{
  
    RevealForward2(p, map); // 안전장치: 이벤트 밟으면 그 주변은 최소 보이게
    char t = map[p->y][p->x];

    if (t == 'C') {
        int first = (p->hasCheckpoint == 0);
        p->hasCheckpoint = 1;
        p->cpX = p->x;
        p->cpY = p->y;
        if (first) {
            p->HP += 30;                      
            if (p->HP > p->maxHP) p->HP = p->maxHP;
            renderMessage("체크포인트 최초 등록! 체력 +30. 아무 키.");
        }
        else {
            renderMessage("체크포인트 갱신! 아무 키.");
        }

        wait_any_key();

        DB_BeginFrame();
        drawUIFrame();
        playerstate(p);
        DrawPassiveUI(p);
        renderSkillBookObject();
        renderMaze(p, map);
        RevealForward2(p, map);
        DrawTopInfo(p);
        gotoxy(42, 22);
        printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
        renderMessage(pickRandomLine());
        renderAction(0);
        DB_Present();

        return;
    }

    if (t == 'B') {
        renderMessage("상자를 발견했다! 스킬 1개 해금, 소량의 골드. 아무 키.");
        wait_any_key();
        UnlockOneSkill(p, p->floor);
        p->gold += 35;
        map[p->y][p->x] = '.';
        return;
    }

    if (t == 'S') {
      
        renderMessage("상인을 만났다! (아무 키)");
        wait_any_key();

        ClearPendingInputSafe();
        PlaySound(TEXT("sound/bgm/lost_haven_shillings.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
        OpenShop(p);
        ClearPendingInputSafe();
        PlaySound(TEXT("sound/bgm/Unknowable.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
        // 메인화면다시 생성
        DB_BeginFrame();
        drawUIFrame();
        playerstate(p);
        DrawPassiveUI(p);
        renderSkillBookObject();
        RevealForward2(p, map);
        DrawTopInfo(p);
        gotoxy(42, 22);
        printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
       
        renderMaze(p, map);
        renderMessage("상점을 나왔다. (아무 키)");
        DB_Present();
        (void)_getch();

        return;
    }


    if (t == 'I') {
        int j = GetJournalIndex(p->floor, p->x, p->y);
        if (j != -1) {
            HandleJournalEvent(p, p->floor, j);
            wait_any_key();
            DB_BeginFrame();
            drawUIFrame();
            DrawPassiveUI(p);
            playerstate(p);
            renderSkillBookObject();
            DrawTopInfo(p);
            gotoxy(42, 22);
            printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
            RevealForward2(p, map);
            renderMaze(p, map);
            DB_Present();
            map[p->y][p->x] = '.';
        }
        else {
            renderMessage("이 일지는 좌표표에 등록이 안 됨!");
            wait_any_key();
        }
        return;
    }

    if (t == 'U') {
        renderMessage("제단 발견! 선택이 가능하다. 아무 키.");
        wait_any_key();
        AltarEvent(p);
        system("cls");
        drawUIFrame();
        playerstate(p);
        DrawPassiveUI(p);
        renderSkillBookObject();
        DrawTopInfo(p);
        gotoxy(42, 22);
        printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
        RevealForward2(p, map);
        renderMaze(p, map);
        map[p->y][p->x] = '.';
        return;
    }

    if (t == '!') {
        renderMessage("강적의 기척이 느껴진다..  전투 시작. 아무 키.");
        wait_any_key();
        PlaySound(TEXT("sound/bgm/alllmer_bells.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
        battleLoop(p, ENC_ELITE, map);
        PlaySound(TEXT("sound/bgm/Unknowable.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
        DrawTopInfo(p);
        map[p->y][p->x] = '.';
        return;
    }

    if (t == '@') {
       
        if (p->floor == 3) { 
            renderMessage("…… '???'.  아무 키.");
            wait_any_key();
            PlaySound(TEXT("sound/bgm/secret_song.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
            battleLoop(p, ENC_BOSS, map);
            PlaySound(TEXT("sound/bgm/Unknowable.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
            
            return;
        }
        renderMessage("지금까지 보았던 적들과 확연히 다르다.  아무 키.");
        wait_any_key();
        PlaySound(TEXT("sound/bgm/horns_and_anxiety.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
        battleLoop(p, ENC_BOSS, map);
        PlaySound(TEXT("sound/bgm/Unknowable.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
        map[p->y][p->x] = '.';
        return;
    }

    if (t == 'G') {
        DoGEvent(p);      
        map[p->y][p->x] = '.';  
        return;          
    }


    if (t == 'E') {
        p->floor++;
        if (p->floor >= FLOORS) p->floor = FLOORS - 1; // 안전
        if (p->floor == 1) {
            showNarration2(p);
        }
        
        LoadMapForFloor(p->floor, map);
        p->x = 1;
        p->y = 1;
        RevealForward2(p, map);
        drawUIFrame();
        playerstate(p);
        DrawPassiveUI(p);
        renderSkillBookObject();
        renderMaze(p, map);
        DrawTopInfo(p);
        renderMessage("다음 층으로 이동했다. 아무 키.");
        wait_any_key();
    }
}

void ApplyJournalVisibility(Player* p, char map[H][W + 1], int floor)
{
    for (int i = 0; i < 3; i++) {
        if (p->journalMask[floor] & (1 << i)) {
            int x = JOURNAL_POS[floor][i].x;
            int y = JOURNAL_POS[floor][i].y;
            if (x >= 0 && x < W && y >= 0 && y < H) map[y][x] = '.';
        }
    }
}

// 일지 아카이브
void OpenJournalArchive(Player* p, char map[H][W + 1])
{
    int floorPage = p->floor;
    int sel = 0;
    int maxJ = 3;
    int needRedraw = 1;
    int first = 1;

    while (1) {
        if (needRedraw) {
            if (first) {
                drawbox(15, 4, 130, 36, " 일지 아카이브 ");
                clearBoxInner(15, 4, 130, 36);
                first = 0;
            }

            clearArea(18, 6, 120, 1);
            gotoxy(18, 6);
            printf("A/D: 층 이동 | ↑↓: 선택 | ENTER: 보기 | ESC: 닫기 (층 %d, 읽음 %d/3)",
                floorPage + 1, CountReadJournals(p, floorPage));

            clearArea(18, 10, 120, 6);
            for (int i = 0; i < maxJ; i++) {
                int y = 10 + i * 2;
                int read = (p->journalMask[floorPage] & (1u << i)) ? 1 : 0;
                gotoxy(20, y);
                printf("%c %d) %s", (i == sel) ? '>' : ' ', i + 1, read ? "읽은 일지" : "???? (미발견)");
            }

            needRedraw = 0;
            DB_Present();
        }

        int ch = _getch();

        if (ch == 27) {
            ClearPendingInputSafe();
            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);
            return;
        }

        if (ch == 'a' || ch == 'A') {
            if (floorPage > 0) floorPage--;
            sel = 0;
            needRedraw = 1;
            continue;
        }

        if (ch == 'd' || ch == 'D') {
            if (floorPage < FLOORS - 1) floorPage++;
            sel = 0;
            needRedraw = 1;
            continue;
        }

        if (ch == 13) {
            int read = (p->journalMask[floorPage] & (1u << sel)) ? 1 : 0;
            if (!read) {
                renderMessage("아직 발견하지 못한 일지다. 아무 키.");
                wait_any_key();
                continue;
            }

            DrawJournal(floorPage, sel);

            ClearPendingInputSafe();
            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            first = 1;
            needRedraw = 1;
            continue;
        }

        if (ch == 224 || ch == 0) {
            int k = _getch();
            int prev = sel;

            if (k == 72) {
                sel--;
                if (sel < 0) sel = maxJ - 1;
            }
            else if (k == 80) {
                sel++;
                if (sel >= maxJ) sel = 0;
            }

            if (sel != prev) needRedraw = 1;
        }
    }
}

// q나래이션
void qnarration(Player* p)
{
    NarrHeader();
    printf("                                                  아직 죽을 이유는 없다.");
    waitenter();
    DB_BeginFrame();
    drawUIFrame();
    playerstate(p);
    DrawPassiveUI(p);
    renderSkillBookObject();
    DrawTopInfo(p);
    gotoxy(42, 22);
    printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
    renderMessage(pickRandomLine());
    renderAction(0);
    DB_Present();
}

void qnarration_end(Player* p)
{
    NarrHeader();
    printf("                                                  목숨을 끊는다.");
}
// 던전 루프
void dungeonLoop(Player* p)
{
   
    char map[H][W + 1];
    LoadMapForFloor(p->floor, map);
    PlaySound(TEXT("sound/bgm/Unknowable.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
    if (p->x == 0 && p->y == 0) {
        p->x = 1;
        p->y = 1;
       
    }
    
    RevealForward2(p, map);
    DB_BeginFrame();
    drawUIFrame();
    playerstate(p);
    DrawPassiveUI(p);
    renderSkillBookObject();
    
    renderMaze(p, map);
    renderMessage(pickRandomLine());
    renderAction(0);

    gotoxy(42, 22);
    printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
    gotoxy(42, 3);
    printf("%d층", p->floor + 1);
    DrawTopInfo(p);
    DB_Present();

    while (1) {
      
        int moved = 0;
        int ch = readKey();

        if (ch == 'q' || ch == 'Q') {
            if (p->floor < 4) {
                // 3층까지는 종료 불가 나래이션만
                qnarration(p);   // "여기서는 죽을 수 없다" 같은 대사만 출력
                continue;                 // 루프 계속
            }
            else {
                // 4층에서부터는 실제 종료
                qnarration_end(p);        // 필요하면 "게임이 끝난다" 대사
                break;                    // 또는 exit(0);
            }
        }

        if (ch == 'w' || ch == 'W') moved = movefront(p, map);
        else if (ch == 's' || ch == 'S') moved = moveBack(p, map);
        if (moved) {
            RevealForward2(p, map);
        }
        else if (ch == 'a' || ch == 'A') turnL(p);
        else if (ch == 'd' || ch == 'D') turnR(p);

        if (ch == 'e' || ch == 'E') {
            OpenCodexScene(p);
            ClearPendingInputSafe();

            DB_BeginFrame();
            drawUIFrame();
            playerstate(p);
            DrawPassiveUI(p);
            renderSkillBookObject();

            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            gotoxy(42, 22);
            printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
            gotoxy(42, 3);
            printf("%d층", p->floor + 1);

            DrawTopInfo(p);
            DB_Present();
            continue;
        }
        if (ch == 'r' || ch == 'R') {
            DB_UpdateWindowInfo();
            DB_BeginFrame();
            // 타이틀 다시 그리기
            drawUIFrame();
            playerstate(p);
            gotoxy(42, 22);
            printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
            gotoxy(42, 3);
            printf("%d층", p->floor + 1);
            DrawPassiveUI(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            

            DrawTopInfo(p);
        }
        if (ch == 'j' || ch == 'J') {
            OpenJournalArchive(p, map);
            ClearPendingInputSafe();

            DB_BeginFrame();
            drawUIFrame();
            playerstate(p);
            DrawPassiveUI(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            gotoxy(42, 22);
            printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
            gotoxy(42, 3);
            printf("%d층", p->floor + 1);

            DrawTopInfo(p);
            DB_Present();
            continue;
        }

        if (ch == 'p' || ch == 'P') {
            OpenPotionMenu(p);
            ClearPendingInputSafe();

            DB_BeginFrame();
            drawUIFrame();
            playerstate(p);
            DrawPassiveUI(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            gotoxy(42, 22);
            printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
            gotoxy(42, 3);
            printf("%d층", p->floor + 1);

            DrawTopInfo(p);
            DB_Present();
            continue;
        }

        renderMaze(p, map);
        renderMessage(pickRandomLine());
        renderAction(ch);
        playerstate(p);
        DrawPassiveUI(p);
        renderSkillBookObject();

        if (moved) {
            HandleTileEvent(p, map);

            if (checkEncounterAfterSafeMoves(p)) {
                renderMessage("적의 기척이 느껴진다... 아무 키.");
                wait_any_key();

                battleLoop(p, ENC_NORMAL, map);

                moveCountSinceLastBattle = 0;
                RevealForward2(p, map);
                DB_BeginFrame();
                drawUIFrame();
                playerstate(p);
                DrawPassiveUI(p);
                renderSkillBookObject();
                renderMaze(p, map);
                renderMessage(pickRandomLine());
                renderAction(0);

                gotoxy(42, 22);
                printf("이동:W S | 방향전환:A D | 스킬 북:E | 일지:J |물약:P | 목숨을끊는다:Q");
                gotoxy(42, 3);
                printf("%d층", p->floor + 1);

                DrawTopInfo(p);
                DB_Present();
                continue;

            }
        }
        DB_Present();
    }
}

// --------------------
// 메인
// --------------------
int main()
{
  


    // 해야할것
    // 고정이벤트(시간이 없다. 폐기)
    // 엔딩 루트 (폐기 시간 없음)
    // 5층 보스추가 (완)(근데약간 문제있음)
    // 나래이션 화면 (수정이 안됨, 그냥 알리는 걸로)
    // 밸런스가 조금 이상함 쉬운지 모르겠음
    // 카드 밸런스는 이정도면 됬다
    // 약화가 적용이 안됨미칠거같음
    // 독이 좀 약한듯(수정할지도)

    Player player;
    memset(&player, 0, sizeof(player));

    setConsoleSize(160, 45);
    showCursor(0);

    DB_Init();
    DB_PrimeBeforeTitle();
    startback();
    showStartScreen();

    showNarration(&player);

    srand((unsigned)time(NULL));
    InitCodex();

    //strcpy_s(player.name, sizeof(player.name), "방랑자");
    player.gold = 20;

    player.maxHP = 65;
    player.HP = player.maxHP;

    player.x = 1;
    player.y = 1;

    player.dir = SOUTH;

    player.level = 1;
    player.exp = 0;
    player.expToNext = 10;

    player.floor = 0;
    player.hasCheckpoint = 0;
    player.cpX = 1;
    player.cpY = 1;

    for (int i = 0; i < MAX_EQUIP; i++) player.equip[i] = -1;
    player.equipCount = 0;

    // 시작 장착: 기본 3개(공격/방어/회복)
    player.equip[0] = 0;
    player.equip[1] = 1;
    player.equip[2] = 2;
    RecountEquip(&player);

    for (int i = 0; i < MAX_PASSIVE; i++) player.passiveSlots[i] = -1;
    player.passiveCount = 0;

    for (int i = 0; i < POT_COUNT; i++) player.potions[i] = 0;
    player.gritUsed = 0;
    drawUIFrame();
    playerstate(&player);
    DrawPassiveUI(&player);
    renderSkillBookObject();

    dungeonLoop(&player);
    return 0;
}