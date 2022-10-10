#include <stdio.h>

// ITT는 기존에 설정된 ITT값을 넣는다.
// TODO: ITT값을 기존의 코드에서 어떤것으로 넣어야 할지 결정해야 한다. 
int mod_ITT  (int ITT) {
    // 내부에서 정의되어야 하는 함수는 CBR, VD, X의 값이다. 
    int CBR;
    int VD;
    int X;  // X = CBR x VD
    int a;  // a는 변경된 ITT의 값(최종 출력할 숫자)

    // CBR = 메시지를 전부 보내는 시간 / 1초(설정된 관측시간)
    // TODO: 메시지 보내는 시간 결정, 
    CBR = 0.8;

    // 한 그룹내의 차량 대수 or 총 차량 대수(10~30)
    VD = 10;

    X = CBR * VD;

    // if문을 사용하여(X범위에 따른 mod_ITT 값 도출)
    if (X <= 6.5) {
        return a = 100;
    } else if (6.5 < X <= 14) {
        return a = 334;
    } else if (14 < X <= 25.5) {
        return a = 500;
    } else {
        return a = 1000;
    };

    return a;
}