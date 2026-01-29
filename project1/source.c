
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#include <stdio.h>
#include <conio.h>
#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#define STAT_H 10

#define DECK_BOX_X 3
#define DECK_BOX_Y 16
#define DECK_BOX_W 25
#define DECK_BOX_H 16

#define CARD_UI_X (UI_W - 30)
#define CARD_UI_Y 4

#define CODEX_COLS 6
#define CODEX_ROWS 3
#define CODEX_SLOTS (CODEX_COLS * CODEX_ROWS) //18

#define SLOT_W 22
#define SLOT_H 8

#define CODEX_X 6
#define CODEX_Y 4

#define SLOT_GAP_X 3
#define SLOT_GAP_Y 1

#define FLOORS 3
#define SKILLS_PER_FLOOR 18
#define MAX_SKILLS (FLOORS * SKILLS_PER_FLOOR) // 54


#define MAX_EQUIP 6
#define ENERGY_MAX 3

int moveCountSinceLastBattle = 0;

// --------------------
// 입력 대기(딜레이 대신)
// --------------------
void wait_any_key() {
    _getch();
}

// --------------------
// 방향/플레이어/적
// --------------------
typedef enum { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 } Dir;

typedef struct {
    int x, y;
    int HP, maxHP;
    char name[50];
    Dir dir;

    int level;
    int exp;
    int expToNext;

    // 장착(전투 사용) 슬롯 6개
    int equip[MAX_EQUIP]; // 스킬 id, 비어있으면 -1
    int equipCount;

    // 체크포인트
    int cpX, cpY;
    int hasCheckpoint;

    // 현재 층(0..2)
    int floor;

    int poison;   // 독 스택
    int burn;     // 화상 스택
    int vuln;     // 취약 턴
    int weak;     // 약화 턴
} Player;

typedef enum {
    EFF_NONE = 0,
    EFF_POISON,
    EFF_BURN,
    EFF_VULN,
    EFF_WEAK
} EffectType;

typedef struct {
    char name[20];
    int hp;
    int maxHp;
    int atk;
    int intent;
    int expReward;
    int poison;   // 독 스택
    int burn;     // 화상 스택
    int vuln;     // 취약 턴
    int weak;     // 약화 턴
} Enemy;

// --------------------
// 스킬(도감 DB 60개)
// --------------------
typedef struct {
    int id;
    char name[32];
    int cost;        // 비용(에너지)
    int dmg, block, heal;
    int unlocked;    // 0=잠김, 1=해금
    char dec[80];

    EffectType eff;   // 상태이상 종류
    int effVal;       // 스택 or 턴 수
} Skill;
Skill codexSkills[MAX_SKILLS];
int codexCount = MAX_SKILLS;

// 상태이상 처리
int CalcPlayerDamage(int base, Enemy* e, Player* p)
{
    int dmg = base;

    // 약화: 주는 피해 감소
    if (p->weak > 0)
        dmg = dmg * 75 / 100;

    // 취약: 받는 피해 증가
    if (e->vuln > 0)
        dmg = dmg * 150 / 100;

    // 독: 공격 시 추가 피해
    if (e->poison > 0)
        dmg += e->poison;

    if (dmg < 0) dmg = 0;
    return dmg;
}
// --------------------
// 콘솔 유틸
// --------------------
char dirchar(Dir d) {
    switch (d) {
    case NORTH: return '^';
    case EAST:  return '>';
    case SOUTH: return 'v';
    case WEST:  return '<';
    }
    return '?';
}

void gotoxy(int x, int y) {
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

void showCursor(int show) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = show ? TRUE : FALSE;
    SetConsoleCursorInfo(h, &info);
}

void setConsoleSize(int width, int height) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

    COORD bufferSize = { (SHORT)width, (SHORT)height };
    SetConsoleScreenBufferSize(h, bufferSize);

    SMALL_RECT windowSize = { 0, 0, (SHORT)(width - 1), (SHORT)(height - 1) };
    SetConsoleWindowInfo(h, TRUE, &windowSize);
}

void drawbox(int x, int y, int w, int h, const char* title) {
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

void clearBoxInner(int x, int y, int w, int h) {
    for (int r = 1; r < h - 1; r++) {
        gotoxy(x + 1, y + r);
        for (int i = 0; i < w - 2; i++) putchar(' ');
    }
}

void ClearPendingInputSafe()
{
    while (_kbhit()) {
        int c = _getch();
        if (c == 224 || c == 0) { // 방향키 2바이트 처리
            if (_kbhit()) _getch();
        }
    }
}

void ClearScreenNoFlicker()
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD written;
    COORD home = { 0,0 };

    GetConsoleScreenBufferInfo(h, &csbi);
    DWORD size = csbi.dwSize.X * csbi.dwSize.Y;

    FillConsoleOutputCharacter(h, ' ', size, home, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, size, home, &written);
    SetConsoleCursorPosition(h, home);
}


void drawUIFrame() {
    system("cls");
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

void sw() {
    gotoxy(6, 33);
    printf("        ");
    for (int i = 0; i <= 130; i++) printf("-");

    gotoxy(6, 34); printf("       /");
    gotoxy(6, 35); printf("      /");
    gotoxy(6, 36); printf("     / ");
    gotoxy(6, 37); printf("    /");
    gotoxy(6, 38); printf("   /");
    gotoxy(6, 39); printf("  /");
    gotoxy(6, 40); printf(" /");
    gotoxy(6, 41); printf("/");
    gotoxy(5, 42);
    for (int i = 0; i <= 134; i++) printf("-");
}

void renderMessageLine(int line, const char* msg) {
    int y = MSG_Y + 2 + line;
    gotoxy(MSG_X + 2, y);
    printf("%-66s", msg ? msg : "");
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


void renderMessage(const char* msg) {
    renderMessageLine(0, msg);
}

void playerstate(Player* p) {
    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%-22s", "");
    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%s", p->name);

    gotoxy(STAT_X + 2, STAT_Y + 3);
    printf("레벨 %d  경험치 %d/%d   ", p->level, p->exp, p->expToNext);

    gotoxy(STAT_X + 2, STAT_Y + 5);
    printf("체력 %d/%d          ", p->HP, p->maxHP);

    gotoxy(STAT_X + 2, STAT_Y + 7);
    printf("층 %d                ", p->floor + 1);

    gotoxy(STAT_X + 2, STAT_Y + 8);
    if (p->hasCheckpoint) printf("체크포인트 O         ");
    else                 printf("체크포인트 X         ");
}

void renderSkillBookObject() {
    int x = DECK_BOX_X + 3;
    int y = DECK_BOX_Y + 2;

    gotoxy(x, y + 0);  printf("    ______________");
    gotoxy(x, y + 1);  printf("   /___________/||");
    gotoxy(x, y + 2);  printf("  +------------+||");
    gotoxy(x, y + 3);  printf("  |            |||");
    gotoxy(x, y + 4);  printf("  |  스 킬  북 |||");
    gotoxy(x, y + 5);  printf("  |            |||");
    gotoxy(x, y + 6);  printf("  |            |||");
    gotoxy(x, y + 7);  printf("  |            |||");
    gotoxy(x, y + 8);  printf("  |            |||");
    gotoxy(x, y + 9);  printf("  |            |||");
    gotoxy(x, y + 10); printf("  +------------+/");
}

// --------------------
// 장착(6개) 유틸
// --------------------
int IsEquipped(Player* p, int skillId) {
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == skillId) return 1;
    }
    return 0;
}

int FirstEmptyEquipSlot(Player* p) {
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == -1) return i;
    }
    return -1;
}

void RecountEquip(Player* p) {
    int c = 0;
    for (int i = 0; i < MAX_EQUIP; i++) if (p->equip[i] != -1) c++;
    p->equipCount = c;
}

void UnequipSkill(Player* p, int skillId) {
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == skillId) {
            p->equip[i] = -1;
            break;
        }
    }
    RecountEquip(p);
}

void EquipSkill(Player* p, int skillId) {
    if (IsEquipped(p, skillId)) return;

    int slot = FirstEmptyEquipSlot(p);
    if (slot != -1) {
        p->equip[slot] = skillId;
        RecountEquip(p);
        return;
    }

    // 꽉 차 있으면: 교체 선택 UI (1~6)
    gotoxy(2, 26);
    renderMessage("슬롯이 가득 찼다. 교체할 슬롯(1~6) 입력, ESC 취소");
    int ch = _getch();
    if (ch == 27) return;

    if (ch >= '1' && ch <= '6') {
        int idx = ch - '1';
        p->equip[idx] = skillId;
        RecountEquip(p);
    }
}

void DrawEquippedList(Player* p, int x, int y) {
    gotoxy(x, y);
    printf("장착(전투 사용) 스킬 6개:");
    for (int i = 0; i < MAX_EQUIP; i++) {
        gotoxy(x, y + 1 + i);
        if (p->equip[i] == -1) {
            printf("%d) (비어있음)                 ", i + 1);
        }
        else {
            Skill* s = &codexSkills[p->equip[i]];
            printf("%d) %-18s(비용:%d)     ", i + 1, s->name, s->cost);
        }
    }
    gotoxy(x, y + 8);
    printf("아무 키: 닫기");
}

// --------------------
// 도감
// --------------------
void ShowSkillDetail(Skill s) {
    ClearScreenNoFlicker();
    drawbox(20, 5, 120, 30, " 스킬 상세 ");

    if (!s.unlocked) {
        gotoxy(24, 10); printf("잠김 상태");
        gotoxy(24, 10); printf("해금이 필요합니다.");
        gotoxy(24, 28); printf("아무 키: 돌아가기");
        wait_any_key();
        return;
    }

    gotoxy(24, 9);  printf("이름 : %s", s.name);
    gotoxy(24, 11); printf("비용 : %d", s.cost);
    gotoxy(24, 13); printf("피해 : %d", s.dmg);
    gotoxy(24, 14); printf("방어 : %d", s.block);
    gotoxy(24, 15); printf("회복 : %d", s.heal);
    gotoxy(24, 18); printf("설명 : %s", s.dec);

    gotoxy(24, 28); printf("아무 키: 돌아가기");
    wait_any_key();
}

void DrawCodexSlot(int x, int y, int w, int h,
    const Skill* s, int hasSkill, int selected, int equipped)
{
    // 테두리
    gotoxy(x, y); putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    for (int r = 1; r < h - 1; r++) {
        gotoxy(x, y + r); putchar('|');
        gotoxy(x + 1, y + r);
        for (int i = 0; i < w - 2; i++) putchar(' ');
        gotoxy(x + w - 1, y + r); putchar('|');
    }

    gotoxy(x, y + h - 1); putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    // ===== 선택표시: 반드시 "지우기"까지 처리해야 잔상 안 남음 =====
    gotoxy(x - 2, y + h / 2); printf("%c", selected ? '>' : ' ');
    gotoxy(x + w + 1, y + h / 2); printf("%c", selected ? '<' : ' ');

    // 내용
    if (!hasSkill || !s) {
        gotoxy(x + 2, y + 2);
        printf("%-18s", "(EMPTY)");
        gotoxy(x + 2, y + 6);
        printf("%-6s", " "); // 장착표시 자리 지우기
        return;
    }

    if (!s->unlocked) {
        gotoxy(x + 2, y + 2); printf("%-18s", "???? (잠김)");
        gotoxy(x + 2, y + 4); printf("%-18s", "해금 필요");
        gotoxy(x + 2, y + 6); printf("%-6s", " "); // 장착표시 자리 지우기
        return;
    }

    gotoxy(x + 2, y + 2);  printf("%-18.18s", s->name);
    gotoxy(x + 2, y + 3);  printf("비용:%-2d          ", s->cost);
    gotoxy(x + 2, y + 4);  printf("피:%-3d 방:%-3d      ", s->dmg, s->block);
    gotoxy(x + 2, y + 5);  printf("회:%-3d             ", s->heal);

    // ===== 장착표시: 고정폭으로 출력해서 잔상 제거 =====
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
    // 도감 내부만 정리 (전체 cls 금지)
    clearBoxInner(2, 1, UI_W - 4, UI_H - 2);

    gotoxy(6, 2);
    printf("E/ESC: 닫기 | ENTER: 상세 | R: 장착/해제 | T: 장착목록 | 방향키: 이동 | A/D: 층 페이지   (층 %d)      ",
        floorPage + 1);

    for (int i = 0; i < CODEX_SLOTS; i++) {
        RedrawOneCodexSlot(p, floorPage, i, selected);
    }

    // 공지줄도 한번 비워두기(잔상 방지)
    CodexNotice(" ");
}


void OpenCodexScene(Player* p)
{
    int floorPage = p->floor;
    int selected = 0;

    // 처음 들어올 때만 화면 큰 틀 1번
    // system("cls") 대신 drawbox + 내부 클리어로 충분
    drawbox(0, 0, UI_W, UI_H, "");
    drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");

    RedrawCodexPage(p, floorPage, selected);

    while (1) {
        int ch = _getch();

        // 특수키(방향키) 처리
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

            // 선택 바뀐 경우에만 슬롯 2개 갱신
            if (selected != prev) {
                RedrawOneCodexSlot(p, floorPage, prev, selected);
                RedrawOneCodexSlot(p, floorPage, selected, selected);
                CodexNotice(" "); // 공지 잔상 방지
            }
            continue;
        }

        // 닫기
        if (ch == 27 || ch == 'e' || ch == 'E') {
            ClearPendingInputSafe(); // 남은 입력 찌꺼기 제거
            return;
        }

        // 층 페이지 변경 (이건 페이지 전체만 다시 그리면 됨)
        if (ch == 'a' || ch == 'A') {
            if (floorPage > 0) floorPage--;
            selected = 0;
            RedrawCodexPage(p, floorPage, selected);
            continue;
        }
        if (ch == 'd' || ch == 'D') {
            if (floorPage < FLOORS - 1) floorPage++;
            selected = 0;
            RedrawCodexPage(p, floorPage, selected);
            continue;
        }

        // 장착목록
        if (ch == 't' || ch == 'T') {
            system("cls");
            drawbox(10, 5, 140, 30, " 장착 목록 ");
            DrawEquippedList(p, 14, 8);
            wait_any_key();

            // 돌아오면 도감 화면을 "다시" 그려줘야 키가 안 먹는 느낌이 없어짐
            drawbox(0, 0, UI_W, UI_H, "");
            drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
            RedrawCodexPage(p, floorPage, selected);
            continue;
        }

        // 상세
        if (ch == 13) {
            int idx = floorPage * SKILLS_PER_FLOOR + selected;
            if (idx >= 0 && idx < MAX_SKILLS)
                ShowSkillDetail(codexSkills[idx]);

            // 상세창 갔다오면 도감 화면 복구
            drawbox(0, 0, UI_W, UI_H, "");
            drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
            RedrawCodexPage(p, floorPage, selected);
            continue;
        }

        // 장착/해제
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

                // 장착 변경 반영: 현재 슬롯만 다시 그리면 됨
                RedrawOneCodexSlot(p, floorPage, selected, selected);
                // 그리고 공지줄 정리
                CodexNotice(" ");
            }
            continue;
        }
    }
}



// --------------------
// 스킬 DB 54개 초기화
//  - 층1: 0~17 (Attack/Defend/Heal 기본 해금 포함)
//  - 층2: 18~35
//  - 층3: 36~54
// --------------------

void SetSkill(
    int idx,
    const char* name, int cost,
    int dmg, int block, int heal,
    int unlocked,
    const char* dec,
    EffectType eff, int effVal
) {
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
    // 화상
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
}


void InitCodex()
{
    // 기본값 초기화
    for (int i = 0; i < MAX_SKILLS; i++) {
        SetSkill(i, "미정", 99, 0, 0, 0, 0, "아직 없음", EFF_NONE, 0);
    }

    // ====== 층1 스킬 18개 (0~17) ======
    // 시작 해금 3개
    SetSkill(0, "공격", 1, 100, 0, 0, 1, "기본 공격", EFF_NONE, 0);
    SetSkill(1, "방어", 1, 0, 6, 0, 1, "기본 방어", EFF_NONE, 0);
    SetSkill(2, "회복", 2, 0, 0, 10, 1, "기본 회복", EFF_NONE, 0);

    // 공격(개성)
    SetSkill(3, "속공", 0, 6, 0, 0, 0, "0비용 빠른 공격", EFF_NONE, 0);
    SetSkill(4, "찌르기", 1, 8, 0, 0, 0, "독 상태 적에게 추가 피해", EFF_NONE, 0); // 조건부는 코드로 처리
    SetSkill(5, "강타", 2, 18, 0, 0, 0, "취약 적에게 추가 피해", EFF_NONE, 0);     // 조건부는 코드로 처리

    // 독(공격 트리거형 독)
    SetSkill(6, "독 바르기", 1, 4, 0, 0, 0, "독 3 부여", EFF_POISON, 3);
    SetSkill(7, "침식", 1, 0, 0, 0, 0, "독 5 부여", EFF_POISON, 5);
    SetSkill(8, "신경 독소", 2, 6, 0, 0, 0, "독 4 부여", EFF_POISON, 4);

    // 화상(턴 피해형)
    SetSkill(9, "화염 참격", 2, 14, 0, 0, 0, "화상 3 부여", EFF_BURN, 3);
    SetSkill(10, "불씨", 1, 4, 0, 0, 0, "화상 3 부여", EFF_BURN, 3);
    SetSkill(11, "잔불", 0, 0, 0, 0, 0, "0비용 화상 1", EFF_BURN, 1);

    // 디버프(취약/약화)
    SetSkill(12, "약점 노출", 1, 0, 0, 0, 0, "취약 2턴", EFF_VULN, 2);
    SetSkill(13, "기력 붕괴", 1, 6, 0, 0, 0, "약화 2턴", EFF_WEAK, 2);
    SetSkill(14, "압박", 2, 0, 0, 0, 0, "취약 3턴", EFF_VULN, 3);

    // 생존
    SetSkill(15, "집중", 2, 0, 20, 0, 0, "안정적인 방어", EFF_NONE, 0);
    SetSkill(16, "철벽", 2, 10, 16, 0, 0, "최선의 방어는 공격", EFF_NONE, 0);
    SetSkill(17, "흡혈", 2, 10, 0, 15, 0, "공격 후 회복", EFF_NONE, 0);

    // 층2/층3은 나중에 18~53 채우면 됨
}

// --------------------
// 해금(상자/상인/처치) 유틸
// --------------------
int RandomLockedSkillOnFloor(int floor) {
    int start = floor * SKILLS_PER_FLOOR;
    int tries = 200;
    while (tries--) {
        int idx = start + (rand() % SKILLS_PER_FLOOR);
        if (idx >= 0 && idx < MAX_SKILLS && codexSkills[idx].unlocked == 0) return idx;
    }
    // 다 열려 있으면 -1
    return -1;
}

void UnlockOneSkill(Player* p, int floor) {
    int idx = RandomLockedSkillOnFloor(floor);
    if (idx == -1) {
        renderMessage("이 층의 스킬은 이미 전부 해금됨. 아무 키.");
        wait_any_key();
        return;
    }

    codexSkills[idx].unlocked = 1;

    char buf[160];
    sprintf_s(buf, sizeof(buf), "스킬 해금: %s  (도감에 기록됨)  [E]=도감 열기 / 아무키=계속", codexSkills[idx].name);
    renderMessage(buf);

    int ch = _getch();

}


// --------------------
// 적 생성 (층당 5종)
// --------------------
Enemy makeEnemyForFloor(int floor) {
    Enemy e;
    memset(&e, 0, sizeof(e));

    int type = rand() % 5;

    if (floor == 0) {
        const char* names[5] = { "슬라임", "박쥐", "쥐", "해골", "거미" };
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
    return e;
}

// --------------------
// 경험치/레벨업
// --------------------
void gainExp(Player* p, int amount) {
    p->exp += amount;

    while (p->exp >= p->expToNext) {
        p->exp -= p->expToNext;
        p->level++;

        // 성장
        p->maxHP += 10;         // 성장만
        if (p->HP > p->maxHP) p->HP = p->maxHP;
        p->expToNext += 5;


        renderMessage("레벨업! 최대 체력 +10. 아무 키.");
        wait_any_key();
    }
}

// --------------------
// 전투 UI (오른쪽에 장착 6개 스킬 리스트)
// --------------------
void drawSkillCardMini(int x, int y, const Skill* s, int selected) {
    gotoxy(x, y);     printf("+----------------------+");
    gotoxy(x, y + 1); printf("|                      |");
    gotoxy(x, y + 2); printf("|                      |");
    gotoxy(x, y + 3); printf("|                      |");
    gotoxy(x, y + 4); printf("+----------------------+");

    if (selected) {
        gotoxy(x - 2, y + 2);  printf(">>");
        gotoxy(x + 24, y + 2); printf("<<");
    }

    if (!s) {
        gotoxy(x + 2, y + 1); printf("(비어있음)");
        return;
    }

    gotoxy(x + 2, y + 1);
    printf("%-16s", s->name);
    gotoxy(x + 19, y + 1);
    printf("비용:%d", s->cost);

    gotoxy(x + 2, y + 2);
    printf("피:%2d 방:%2d 회:%2d", s->dmg, s->block, s->heal);

    gotoxy(x + 2, y + 3);
    printf("%-20.20s", s->dec);
}

void drawEquippedSkillsUI(Player* p, int selected) {
    int baseX = CARD_UI_X;
    int baseY = CARD_UI_Y;
    int gapY = 5;

    for (int i = 0; i < MAX_EQUIP; i++) {
        Skill* s = NULL;
        if (p->equip[i] != -1) s = &codexSkills[p->equip[i]];
        drawSkillCardMini(baseX, baseY + i * gapY, s, (i == selected));
    }
}

// --------------------
// 전투 로직 (6개 스킬 중 선택 + 에너지/가드)
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

    //  에너지 먼저 차감
    *energy -= s->cost;

    //  공격(딱 한 번만)
    if (s->dmg > 0) {
        int base = s->dmg;

        // 찌르기(4): 독 걸려있으면 +4
        if (s->id == 4 && e->poison > 0) base += 4;

        // 강타(5): 취약 걸려있으면 +10
        if (s->id == 5 && e->vuln > 0) base += 10;

        int real = CalcPlayerDamage(base, e, p);
        e->hp -= real;
        if (e->hp < 0) e->hp = 0;
    }

    // 방어/회복
    if (s->block > 0) {
        *block += s->block;
    }
    if (s->heal > 0) {
        p->HP += s->heal;
        if (p->HP > p->maxHP) p->HP = p->maxHP;
    }

    // 상태이상 부여
    switch (s->eff) {
    case EFF_POISON: e->poison += s->effVal; break;
    case EFF_BURN:   e->burn += s->effVal; break;
    case EFF_VULN:   e->vuln += s->effVal; break;
    case EFF_WEAK:   e->weak += s->effVal; break;
    default: break;
    }

    char buf[160];
    sprintf_s(buf, sizeof(buf),
        "사용: %s | 남은에너지:%d | 적체력:%d/%d  (아무 키)",
        s->name, *energy, e->hp, e->maxHp);

    renderMessage(buf);
    wait_any_key();
    return 1;
}


void enemyTurn(Player* p, Enemy* e, int* block) {
    // 의도 결정
    e->intent = rand() % 3; // 0 공격, 1 강공, 2 방어

    if (e->intent == 2) {
        // 적 방어(간단히 atk 상승 같은 걸로 바꿔도 됨)
        renderMessage("적이 수비 자세를 취한다. 아무 키.");
        wait_any_key();
        // (아직 실제 방어막 구현 안 함, enemyBlock 넣으면 됨)
        return;
    }

    int dmg = e->atk + (e->intent == 1 ? 5 : 0);

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
    if (e->intent == 1) sprintf_s(buf, sizeof(buf), "%s의 강공! 피해:%d  (아무 키)", e->name, dmg);
    else               sprintf_s(buf, sizeof(buf), "%s의 공격! 피해:%d  (아무 키)", e->name, dmg);

    renderMessage(buf);
    wait_any_key();

    // 턴 종료 시 방어막 사라짐(슬더스식) - 네가 원하면 유지형으로 바꿔도 됨
    *block = 0;
}

// 전투에서 죽으면 체크포인트로 워프 + 체력 회복
void RespawnAtCheckpoint(Player* p) {
    if (p->hasCheckpoint) {
        p->x = p->cpX;
        p->y = p->cpY;
    }
    else {
        // 체크포인트 없으면 시작점으로(1,1)
        p->x = 1;
        p->y = 1;
    }
    p->HP = p->maxHP;
}

int battleLoop(Player* p) {
    Enemy enemy = makeEnemyForFloor(p->floor);

    int energy = ENERGY_MAX;
    int block = 0;
    int selected = 0;
    int zeroUsedThisTurn = 0;

    while (1) {
        // 승패 체크
        if (enemy.hp <= 0) {
            renderMessage("승리! 경험치를 획득했다. 아무 키.");
            wait_any_key();
            gainExp(p, enemy.expReward);

            // 적 처치 보상(확률로 해금 1개)
            if ((rand() % 100) < 35) {
                renderMessage("보상 발견! 스킬 1개 해금. 아무 키.");
                wait_any_key();
                UnlockOneSkill(p, p->floor);
            }
            return 1; // 승리
        }

        if (p->HP <= 0) {
            renderMessage("패배... 체크포인트로 이동한다. 아무 키.");
            wait_any_key();
            RespawnAtCheckpoint(p);
            return 0; // 패배
        }

        // 화면
        drawUIFrame();
        sw();
        playerstate(p);
        renderSkillBookObject();

        clearBoxInner(M_X, M_Y, M_W, M_H);
        gotoxy(M_X + 2, M_Y + 2);
        printf("<< 전투 >>");

        gotoxy(M_X + 2, M_Y + 4);
        printf("적: %-10s  체력:%3d/%3d", enemy.name, enemy.hp, enemy.maxHp);

        gotoxy(M_X + 2, M_Y + 8);
        printf("에너지:%d/%d   방어막:%d", energy, ENERGY_MAX, block);

        gotoxy(M_X + 2, M_Y + 18);
        printf("조작: ↑↓ 선택 | ENTER 사용 | SPACE 턴종료 | ESC 도주(임시)");

        gotoxy(M_X + 2, M_Y + 6);
        printf("상태: ");

        if (enemy.poison > 0) printf("[독%d] ", enemy.poison);
        if (enemy.burn > 0) printf("[화%d] ", enemy.burn);
        if (enemy.vuln > 0) printf("[취%d] ", enemy.vuln);
        if (enemy.weak > 0) printf("[약%d] ", enemy.weak);

        // 오른쪽: 장착 6개 표시
        drawEquippedSkillsUI(p, selected);

        int ch = _getch();

        if (ch == 27) { //esc
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

                energy = ENERGY_MAX;
                zeroUsedThisTurn = 0;
                block = 0;

                continue;
            }

        }

        // 턴 종료 → 적 턴 → 내 턴 에너지 회복
        if (ch == ' ') { // 턴 종료
            // 1️ 플레이어 턴 종료 처리 (화상, 취약 감소 등)
            ApplyTurnEnd_Player(p);

            // 2️ 적 턴 실행
            enemyTurn(p, &enemy, &block);

            // 3️ 적 턴 종료 처리 (화상, 취약 감소 등)
            ApplyTurnEnd_Enemy(&enemy);

            // 4️ 다음 플레이어 턴 준비
            energy = ENERGY_MAX;
            zeroUsedThisTurn = 0;
            block = 0;   // 방어도는 턴 종료 시 초기화

            continue;
        }

        // 사용
        if (ch == 13) {
            if (p->equip[selected] == -1) {
                renderMessage("이 슬롯은 비어있다. 도감(E)에서 장착해라. 아무 키.");
                wait_any_key();
            }
            else {
                Skill* s = &codexSkills[p->equip[selected]];

                if (s && s->cost == 0 && zeroUsedThisTurn) {
                    renderMessage("0비용 스킬은 턴당 1회만 사용 가능! (아무 키)");
                    wait_any_key();
                    continue;
                }

                if (applySkill(p, &enemy, &block, &energy, s)) {
                    if (s && s->cost == 0)
                        zeroUsedThisTurn = 1;
                }
                continue;
            }
        }


        // 선택 이동
        if (ch == 224 || ch == 0) {
            ch = _getch();
            if (ch == 72) { // ↑
                selected--;
                if (selected < 0) selected = MAX_EQUIP - 1;
            }
            else if (ch == 80) { // ↓
                selected++;
                if (selected >= MAX_EQUIP) selected = 0;
            }
        }
    }
}

// --------------------
// 랜덤 문장
// --------------------
const char* pickRandomLine() {
    static const char* lines[] = {
        "음습한 던전 속에서 고요함이 느껴진다...",
        "어둠 속에서 청각이 예민해진 기분이다..",
        "이 길이 맞는 것 같은 느낌이든다.",
        "뭔가 수상한 기운이 느껴진다.",
        "무언의 압박감이 느껴진다...",
        "발소리가 들린다.",
        "떨리는 심장의 파동이 온 몸을 관통한다.",
        "정적 속에서 심장 소리만이 또렷이 들린다."
    };
    int count = (int)(sizeof(lines) / sizeof(lines[0]));
    return lines[rand() % count];
}

void renderAction(int ch) {
    gotoxy(MSG_X + 2, MSG_Y + 3);

    if (ch == 'w' || ch == 'W')      printf("%-66s", "앞으로 이동했다");
    else if (ch == 's' || ch == 'S') printf("%-66s", "뒤로 이동했다");
    else if (ch == 'a' || ch == 'A') printf("%-66s", "왼쪽을 바라본다");
    else if (ch == 'd' || ch == 'D') printf("%-66s", "오른쪽을 바라본다");
    else if (ch == 'f' || ch == 'F') printf("%-66s", "층을 이동한다");
    else if (ch == 'e' || ch == 'E') printf("%-66s", "도감을 연다");
    else                              printf("%-66s", "...");
}

// --------------------
// 고정 인카운터 처리(B/S/C)
// --------------------
void HandleTileEvent(Player* p, char map[H][W + 1]) {
    char t = map[p->y][p->x];

    if (t == 'C') {
        p->hasCheckpoint = 1;
        p->cpX = p->x;
        p->cpY = p->y;
        renderMessage("체크포인트 등록! 죽으면 여기로 돌아온다. 아무 키.");
        wait_any_key();
        return;
    }

    if (t == 'B') {
        renderMessage("상자를 발견했다! 스킬 1개 해금. 아무 키.");
        wait_any_key();
        UnlockOneSkill(p, p->floor);

        // 상자는 1회성 처리
        map[p->y][p->x] = '.';
        return;
    }

    if (t == 'S') {
        renderMessage("상인을 만났다! (스킬 1개 해금) 아무 키.");
        wait_any_key();
        UnlockOneSkill(p, p->floor);
        map[p->y][p->x] = '.';
        // 상인은 고정(유지)
        return;
    }
}


// 이동
void turnL(Player* p) { p->dir = (Dir)((p->dir + 3) % 4); }
void turnR(Player* p) { p->dir = (Dir)((p->dir + 1) % 4); }

void wfront(Dir d, int* fx, int* fy) {
    *fx = 0; *fy = 0;
    if (d == NORTH) *fy = -1;
    else if (d == SOUTH) *fy = 1;
    else if (d == WEST)  *fx = -1;
    else if (d == EAST)  *fx = 1;
}

int movefront(Player* p, char map[H][W + 1]) {
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

int moveBack(Player* p, char map[H][W + 1]) {
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

int checkEncounterAfterSafeMoves(void) {
    moveCountSinceLastBattle++;
    if (moveCountSinceLastBattle <= 6) return 0;
    return (rand() % 100) < 35;
}

void renderMaze(Player* p, char map[H][W + 1]) {
    int ox = M_X + 2;
    int oy = M_Y + 2;

    for (int y = 0; y < H; y++) {
        gotoxy(ox, oy + y);

        for (int x = 0; x < W; x++) {
            if (p->x == x && p->y == y) {
                printf("%c ", dirchar(p->dir));
            }
            else if (map[y][x] == '#') {
                printf("##");
            }
            else {
                char t = map[y][x];
                if (t == 'B' || t == 'S' || t == 'C') printf("%c ", t);
                else printf("  ");
            }
        }
    }
}


// 층별 맵(예시: 모양 같고 고정 인카운터 위치만 다르게)
void LoadMapForFloor(int floor, char map[H][W + 1]) {
    // 기본 틀
    const char* base[H] = {
        "#################################",
        "#..#..............#.............#",
        "#..#..######..###.#..#####..##..#",
        "#..#..#....#..#...#..#....#..#..#",
        "#..####....####...####....####..#",
        "#......#...............#........#",
        "######..#.#########..#.#######..#",
        "#......#.#...........#.#........#",
        "#.######.#..########.#.#.#####..#",
        "#.#......#..#........#.#.#......#",
        "#.#.########.#.#######.#.#.###..#",
        "#.#............#...........#....#",
        "#..............#................#",
        "#################################"
    };

    for (int y = 0; y < H; y++) {
        strcpy_s(map[y], W + 1, base[y]);
    }

    // 고정 인카운터 위치를 층마다 다르게 ( 더 추가 가능)
    if (floor == 0) {
        map[1][3] = 'C';
        map[5][6] = 'B';
        map[10][27] = 'S';
    }
    else if (floor == 1) {
        map[1][5] = 'C';
        map[7][10] = 'B';
        map[11][24] = 'S';
    }
    else {
        map[1][7] = 'C';
        map[6][18] = 'B';
        map[9][26] = 'S';
    }
}


// 던전 루프
void dungeonLoop(Player* p) {
    char map[H][W + 1];

    LoadMapForFloor(p->floor, map);

    if (p->x == 0 && p->y == 0) { p->x = 1; p->y = 1; }

    drawUIFrame();
    playerstate(p);
    renderSkillBookObject();
    //  sw();
    renderMaze(p, map);
    renderMessage(pickRandomLine());
    renderAction(0);

    while (1) {
        int moved = 0;
        int ch = _getch();

        if (ch == 'Q' || ch == 'q') break;

        if (ch == 'w' || ch == 'W') moved = movefront(p, map);
        else if (ch == 's' || ch == 'S') moved = moveBack(p, map);
        else if (ch == 'a' || ch == 'A') turnL(p);
        else if (ch == 'd' || ch == 'D') turnR(p);

        // 도감 열기(장착 포함)
        if (ch == 'e' || ch == 'E') {
            OpenCodexScene(p);
            ClearPendingInputSafe();

            // 복귀 화면

            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);
            continue;
        }

        // 층 이동(예시: F로 다음 층, 3층이면 1층으로)
        if (ch == 'f' || ch == 'F') {
            p->floor = (p->floor + 1) % FLOORS;
            // 층 이동 시 맵 재로드 + 위치 초기화
            LoadMapForFloor(p->floor, map);
            p->x = 1; p->y = 1;

            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            //  sw();
            renderMaze(p, map);
            renderMessage("층을 이동했다. 아무 키.");
            wait_any_key();
            renderMessage(pickRandomLine());
            continue;
        }

        // 이동/회전 결과 반영
        renderMaze(p, map);
        renderMessage(pickRandomLine());
        renderAction(ch);
        playerstate(p);
        //   sw();
        renderSkillBookObject();

        // 이동했으면 타일 이벤트 처리(B/S/C)
        if (moved) {
            HandleTileEvent(p, map);

            // 전투 인카운터
            if (checkEncounterAfterSafeMoves()) {
                renderMessage("적의 기척이 느껴진다... 아무 키.");
                wait_any_key();

                battleLoop(p); // 승패 처리 내부에서 체크포인트 respawn까지 함
                moveCountSinceLastBattle = 0;

                // 전투 후 화면 복귀
                drawUIFrame();
                playerstate(p);
                renderSkillBookObject();
                // sw();
                renderMaze(p, map);
                renderMessage(pickRandomLine());
                renderAction(0);
            }
        }
    }
}


// 메인
int main() {

    Player player;
    memset(&player, 0, sizeof(player));

    setConsoleSize(160, 45);
    showCursor(0);

    srand((unsigned)time(NULL));
    InitCodex();

    strcpy_s(player.name, sizeof(player.name), "방랑자");
    player.maxHP = 100;
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
    player.equip[0] = 0; // 공격
    player.equip[1] = 1; // 방어
    player.equip[2] = 2; // 회복
    RecountEquip(&player);

    drawUIFrame();
    playerstate(&player);
    renderSkillBookObject();
    // sw();

    dungeonLoop(&player);
    return 0;
}
