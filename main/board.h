#ifndef _BOARD_H__
#define _BOARD_H__

/** 按钮 GPIO：按下为低电平（接 GND），松开为高电平（内部上拉） */
#define BUTTON_GPIO GPIO_NUM_4
/** LED GPIO：按下按钮时亮，松开时灭 */
#define LED_GPIO GPIO_NUM_5

class Board
{
private:
    Board(const Board &) = delete;
    Board &operator=(const Board &) = delete;

    int s_retry_num = 0;

public:
    static Board &getInstance()
    {
        static Board instance;
        return instance;
    };
    Board();
    ~Board();

    void StartNetwork();
    void SetButton();
};

#endif