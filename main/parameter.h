typedef struct
{
	uint16_t port;
	char ipv4[20]; // xxx.xxx.xxx.xxx
} PARAMETER_t;

typedef struct
{
	float quatx;
	float quaty;
	float quatz;
	float quatw;
	float roll;
	float pitch;
	float yaw;
} POSE_t;

typedef struct
{
	int seq;
	float dt;
	int ax;
	int ay;
	int az;
} POSE_a;

typedef struct
{
	int seq;
	int16_t ax;
	int16_t ay;
	int16_t az;
	int16_t gx;
	int16_t gy;
	int16_t gz;
	float qx;
	float qy;
	float qz;
	float qw;
	float roll;
	float pitch;
	float yaw;
} POSE_a_g;
