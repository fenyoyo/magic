#ifndef _BOARD_H__
#define _BOARD_H__

#include <string>

/** 按钮 GPIO：按下为低电平（接 GND），松开为高电平（内部上拉） */
#define BUTTON_GPIO GPIO_NUM_10
#define BUTTON_GPIO_R GPIO_NUM_11
/** LED GPIO：按下按钮时亮，松开时灭 */
#define LED_GPIO GPIO_NUM_5
#define LED_GPIO_R GPIO_NUM_6

class Board
{
private:
    Board(const Board &) = delete;
    Board &operator=(const Board &) = delete;

    int s_retry_num = 0;
    // OLED *m_oled;

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

    // OLED相关方法
    // bool initOLED();
    // OLED *getOLED() { return m_oled; }

    // 设备ID相关方法
    static std::string getDeviceId();
    static std::string getDeviceId2();
};

#endif