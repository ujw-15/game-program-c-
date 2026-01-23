#include <stdio.h>
#include <conio.h>
#include <Windows.h>
#include <stdlib.h>
#include <string.h>

#define H 8
#define W 15

typedef enum {
	NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3
} Dir;

typedef struct {
	int x, y;
	char name[50];
	Dir dir;
} Player;

char dirchar(Dir d) {
	switch (d) {
	case NORTH: return '^';
	case EAST: return '>';
	case SOUTH: return 'v';
	case WEST: return '<';
	}
	return '?'; 
}

void gotoxy(int x, int y) {
	COORD pos = { x, y };

	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

void showCursor(int show)
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO info;
	GetConsoleCursorInfo(h, &info);
	info.bVisible = show ? TRUE : FALSE;
	SetConsoleCursorInfo(h, &info);
}

void setConsoleSize(int width, int height)
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	COORD bufferSize = { (SHORT)width, (SHORT)height };
	SetConsoleScreenBufferSize(h, bufferSize);

	SMALL_RECT windowSize = { 0, 0, (SHORT)(width - 1), (SHORT)(height - 1) };
	SetConsoleWindowInfo(h, TRUE, &windowSize);
}

void turnL(Player* p) {
	p->dir = (Dir)((p->dir + 3) % 4);
}

void turnR(Player* p) {
	p->dir = (Dir)((p->dir + 1) % 4);
}

void wfront(Dir d, int *fx, int *fy)
{
	*fx = 0, * fy = 0;
	if (d == NORTH) *fy = -1;
	else if (d == SOUTH) *fy = 1;
	else if (d == WEST) *fx = -1;
	else if (d == EAST) *fx = 1;
}

void movefront(Player* p, char map[H][W + 1])
{
	
	int fx, fy;
	wfront(p->dir, &fx, &fy);
	
	int nx = p->x + fx;
	int ny = p->y + fy;

	if (nx >= 0 && nx < W
		&& ny >= 0 && ny < H && map[ny][nx] != '#')
	{
		p->x = nx;
		p->y = ny;
	}	
}

void moveBack(Player* p, char map[H][W + 1])
{
	int fx, fy;
	wfront(p->dir, &fx, &fy);

	int nx = p->x - fx;
	int ny = p->y - fy;

	if (nx >= 0 && nx < W
		&& ny >= 0 && ny < H && map[ny][nx] != '#')
	{
		p->x = nx;
		p->y = ny;
	}
}

void waitenter()
{
	int eh;
	

	while (1)
	{
		eh = _getch();
		if (eh == 13) break;
	}
}

void showStartScreen()
{
	system("cls");

	printf("\n\n");
	for (int i = 0; i < 120; i++) {
		printf("=");
	} printf("\n");
	printf("\n\n");

	printf("                                                  DUNGEON GAME\n\n\n");
	for (int i = 0; i < 120; i++) {
		printf("=");
	} printf("\n\n");

	printf("                                            press enter key to start\n");

	waitenter();
}

void showNarration(Player*p)
{
	

	system("cls");
	printf("\n");
	printf("====================================\n");
	printf("              NARRATION             \n");
	printf("====================================\n\n");

	printf("[Enter] 다음\n\n");
	waitenter();
	printf("...눈을 뜨자, 낯선 공기가 폐를 찔렀다.\n\n");
	waitenter();
	printf("벽은 차갑고, 바닥은 젖어 있었다, 앞은 칠흙같은 어둠으로 번져있다.\n\n");
	waitenter();
	printf("어딘가에서, 아주 작은 숨소리가 들린다.\n\n");
	waitenter();
	printf("당신은 그 나약한 숨소리가 본인이라는 것을 인지했다.");
	waitenter();

	system("cls");
	printf("\n");
	printf("====================================\n");
	printf("              NARRATION             \n");
	printf("====================================\n\n");

	printf("당신은 무언가를 잊고 있다.\n\n");
	waitenter();
	printf("이곳에 들어온 이유도, 돌아갈 길도.\n\n");
	waitenter();
	printf("하지만 한 가지는 확실하다.\n\n");
	waitenter();
	printf("여기서 나가야 한다는 것을.\n\n");
	waitenter();
	
	system("cls");
	printf("\n");
	printf("====================================\n");
	printf("              NARRATION             \n");
	printf("====================================\n\n");

	printf("당신은 몸을 쑤셔오는 고통과 공포를 뒤엎고 일어셨다.\n\n");
	waitenter();
	printf("당신은 희미해져가는 머리속에서 자신의 이름을 생각한다.\n\n");
	waitenter();
	system("cls");
	printf("내 이름은....\n\n");
	printf("당신의 이름을 생각하시오: \n\n");

	int c;
	while ((c = getchar()) != '\n' && c != EOF) {}

	fgets((*p).name, sizeof((*p).name), stdin);

	(*p).name[strcspn((*p).name, "\n")] = '\0';

	if ((*p).name[0] == '\0') strcpy_s((*p).name, sizeof((*p).name), "방랑자");
	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
	printf("어지러운 머리속에선 이름만이 기억난다.\n");
	waitenter();

	system("cls");
	printf("\n");
	printf("====================================\n");
	printf("              NARRATION             \n");
	printf("====================================\n\n");

	printf("%s은(는) 근처 상황을 살펴본다.\n\n", (*p).name);
	waitenter();
	printf("보란듯이 놓여져있는 낡은 검과 방패,,\n\n");
	waitenter();
	printf("%s은(는) 꺼림찍함을 뒤로하고 검과 방패를 장비한다,\n\n", (*p).name);
	waitenter();
	printf("%s은(는) 마음을 한차례 심호흡을 한 후 커다란 문을 열고 앞으로 나아간다.", (*p).name);
	waitenter();
	
}

void item()
{
	printf("___________\n");
	printf("|aaaa: 1234|\n");

}

void dungeonLoop(Player*p)
{

	char map[H][W + 1] = {
	"###############",
	"#.#...........#",
	"#.#....#####.##",
	"#.####.#####..#",
	"#......#....#.#",
	"######.#.####.#",
	"#......#......#",
	"###############",
	};

	if ((*p).x == 0 && (*p).y == 0)
	{ (*p).x = 1; (*p).y = 1; };

	while (1)
	{
		gotoxy(0, 0);

		for (int i = 0; i < H; i++) {
			for (int k = 0; k < W; k++) {
				if (i == (*p).y && k == (*p).x)
					printf("%c", dirchar(p->dir));
				else
					printf("%c", map[i][k]);
			}
			printf("\n");
		}

		printf("WA : move\nAD : turn\nQ quit\n");

		int ch = _getch();
		
		if (ch == 'Q' || ch == 'q') break;

		if (ch == 'w' || ch == 'W') movefront(p, map);
		else if (ch == 's' || ch == 'S')  moveBack(p, map);
		else if (ch == 'a' || ch == 'A') turnL(p);
		else if (ch == 'd' || ch == 'D') turnR(p);

	}
}



int main()
{
	Player player = { 0 };

	setConsoleSize(120, 40);
	showCursor(0);

	//showStartScreen();
	//showNarration(&player);
	system("cls");
	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
	player.x = 1; player.y = 1;
	player.dir = SOUTH;
	dungeonLoop(&player);

	return 0;
}
				