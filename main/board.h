#ifndef _BOARD_H__
#define _BOARD_H__

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
};

#endif