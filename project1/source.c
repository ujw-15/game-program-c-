#include <stdio.h>
#include <conio.h>
#include <Windows.h>

#define H 8
#define W 15


typedef struct {
	int x, y;
} Player;

void gotoxy(int x, int y) {
	COORD pos = { x, y };

	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}




int main()
{
	char map[H][W + 1] = {
	"###############",
	"#.............#",
	"#.............#",
	"#.............#",
	"#.............#",
	"#.............#",
	"#.............#",
	"###############",
	};

	Player p = { 1, 1 };

	while (1)
	{
		gotoxy(0, 0);

		for (int i = 0; i < H; i++) {
			for (int k = 0; k < W; k++) {
				if (i == p.y && k == p.x)
					printf("*");
				else
					printf("%c", map[i][k]);
			}
			printf("\n");
		}

		printf("WASD : move, Q quit\n");

		int ch = _getch();
		if (ch == 'Q' || ch == 'q') break;
		
		int ix = p.x, iy = p.y;
		if (ch == 'w' || ch == 'W') iy--;

		else if (ch == 's' || ch == 'S') iy++;

		else if (ch == 'a' || ch == 'A') ix--;

		else if (ch == 'd' || ch == 'D') ix++;

		if (ix >= 0 && ix < W
			&& iy >= 0 && iy < H && map[iy][ix] != '#')
		{
			p.x = ix;
			p.y = iy;
		}

	}
	return 0;
}
				