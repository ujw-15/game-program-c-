#include<stdio.h>
#include<windows.h>

#define SIZE 10

int main()
{
#pragma region 포인터 변수
	//
	//	const char* dialog[SIZE];
	//
	//	dialog[0] = "안녕하세요?";
	//	dialog[1] = "누구세요?";
	//	dialog[2] = "의뢰를 맡은 탐정입니다.";
	//	dialog[3] = "그러시군요";
	//	dialog[4] = "혹시 사건현장을 둘러봐도 되겠습니까?";
	//	dialog[5] = "네 그럼요.";
	//	dialog[6] = "이상하리만치 특별한 점은 없네요";
	//	dialog[7] = "그러게요..";
	//	dialog[8] = "피의자 시체는 어디죠?";
	//	dialog[9] = "....";
	//
	//	// 0x0000 : 이전에 누른 적이 없고 호출 시점에도 눌려있지 않은 상태
	//
	//	// 0x0001 : 이전에 누른 적이 있고 호출 시점에는 눌려있지 않은 상태
	//
	//	// 0x8000 :  이전에 누른 적이 없고 호풀 시점에는 눌려있는 상태
	//
	//	// 0x8001 :  이전에 누른 적이 있고 호풀 시점에도 눌려있는 상태
	//
	//	const char* who[2] = { "탐정","의뢰인" };
	//	int i = 0;
	//	while (i < SIZE)
	//	{
	//		if (GetAsyncKeyState(VK_SPACE) & 0x0001)
	//		{
	//			printf("%s : %s\n", who[i % 2], dialog[i]);
	//			i++;
	//
	//			if (i >= SIZE)
	//				break;
	//
	//			// 풀이
	//
	//			if (i % 2 == 0) {
	//				printf("탐정 : %s\n\n", dialog[i]);
	//			}
	//			else
	//			{
	//				printf("의뢰인 : %s\n\n", dialog[i]);
	//			}
	//			i++;
	//		}
	//	}
	//
	//	printf("대화가 종료되었습니다.");
	//
#pragma endregion


	return 0;
}