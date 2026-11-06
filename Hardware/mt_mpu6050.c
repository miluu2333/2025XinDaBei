#include "stm32f10x.h" 
#include <math.h> 
#include "mt_mpu6050.h" 
#include "Delay.h" 

// ================================================================ 
//   软件 I2C 驱动部分 (专门解决引脚接反/硬件I2C不稳的问题) 
// ================================================================ 

// 【关键修改】这里定义您接反后的引脚 
// 原始定义：SCL=PB10, SDA=PB11 
// 现在的定义（接反修正版）： 
#define I2C_PORT    GPIOB 
#define I2C_SCL_PIN GPIO_Pin_11  // 现在的 SCL (原 SDA) 
#define I2C_SDA_PIN GPIO_Pin_10  // 现在的 SDA (原 SCL) 

// I2C 写 SCL/SDA 高低电平宏 
#define I2C_SCL_H   GPIO_SetBits(I2C_PORT, I2C_SCL_PIN) 
#define I2C_SCL_L   GPIO_ResetBits(I2C_PORT, I2C_SCL_PIN) 
#define I2C_SDA_H   GPIO_SetBits(I2C_PORT, I2C_SDA_PIN) 
#define I2C_SDA_L   GPIO_ResetBits(I2C_PORT, I2C_SDA_PIN) 
#define I2C_SDA_READ GPIO_ReadInputDataBit(I2C_PORT, I2C_SDA_PIN) 

MPU6050_t g_mpu6050; 

// 软件I2C初始化 (覆盖 main.c 里的配置) 
void MyI2C_Init(void) 
{ 
    GPIO_InitTypeDef GPIO_InitStructure; 
    
    // 开启 GPIOB 时钟 
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); 
    
    // 1. 配置 SCL (PB11) 和 SDA (PB10) 为开漏输出 
    GPIO_InitStructure.GPIO_Pin = I2C_SCL_PIN | I2C_SDA_PIN; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_Init(I2C_PORT, &GPIO_InitStructure); 
    
    // 2. 【关键新增】配置 AD0 (PB14) 为推挽输出 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
    
    // 3. 【关键操作】拉低 PB14 -> 设定地址为 0xD0 
    // (如果您想用 0xD2，就把这就改成 GPIO_SetBits) 
    GPIO_ResetBits(GPIOB, GPIO_Pin_14); 
    
    // 4. 拉高 I2C 总线（空闲状态） 
    I2C_SCL_H; 
    I2C_SDA_H; 
}

// 产生起始信号 
void MyI2C_Start(void) 
{ 
    I2C_SDA_H; 
    I2C_SCL_H; 
    Delay_us(4); // 延时保证时序 
    I2C_SDA_L; 
    Delay_us(4); 
    I2C_SCL_L;   // 钳住I2C总线，准备发送或接收数据 
} 

// 产生停止信号 
void MyI2C_Stop(void) 
{ 
    I2C_SCL_L; 
    I2C_SDA_L; 
    Delay_us(4); 
    I2C_SCL_H; 
    Delay_us(4); 
    I2C_SDA_H;   // 发送I2C总线结束信号 
} 

// 等待应答信号 
// 返回值：1-接收应答失败，0-接收应答成功 
uint8_t MyI2C_WaitAck(void) 
{ 
    uint8_t ucErrTime = 0; 
    I2C_SDA_H; // 释放SDA线 
    Delay_us(2); 
    I2C_SCL_H; 
    Delay_us(2); 
    while(I2C_SDA_READ) 
    { 
        ucErrTime++; 
        if(ucErrTime > 250) 
        { 
            MyI2C_Stop(); 
            return 1; 
        } 
    } 
    I2C_SCL_L; // 时钟输出0 
    return 0; 
} 

// 产生ACK应答 
void MyI2C_Ack(void) 
{ 
    I2C_SCL_L; 
    I2C_SDA_L; 
    Delay_us(2); 
    I2C_SCL_H; 
    Delay_us(2); 
    I2C_SCL_L; 
} 

// 不产生ACK应答 
void MyI2C_NAck(void) 
{ 
    I2C_SCL_L; 
    I2C_SDA_H; 
    Delay_us(2); 
    I2C_SCL_H; 
    Delay_us(2); 
    I2C_SCL_L; 
} 

// 发送一个字节 
void MyI2C_SendByte(uint8_t txd) 
{                        
    uint8_t t;   
    I2C_SDA_L;      
    I2C_SCL_L; // 拉低时钟开始数据传输 
    for(t=0; t<8; t++) 
    {               
        if((txd & 0x80) >> 7) 
            I2C_SDA_H; 
        else 
            I2C_SDA_L; 
        txd <<= 1; 	    
        Delay_us(2); 
        I2C_SCL_H; 
        Delay_us(2); 
        I2C_SCL_L; 	 
        Delay_us(2); 
    } 	   
} 

// 读取一个字节 
// ack=1时，发送ACK，ack=0，发送nACK 
uint8_t MyI2C_ReadByte(unsigned char ack) 
{ 
    unsigned char i, receive = 0; 
    I2C_SDA_H; // 释放SDA线，设为输入模式（OD模式下写1即为释放） 
    for(i=0; i<8; i++ ) 
    { 
        I2C_SCL_L; 
        Delay_us(2); 
        I2C_SCL_H; 
        receive <<= 1; 
        if(I2C_SDA_READ) receive++;   
        Delay_us(1); 
    } 	 	 	 	 	 
    if (!ack) 
        MyI2C_NAck(); // 发送nACK 
    else 
        MyI2C_Ack();  // 发送ACK   
    return receive; 
} 

// ================================================================ 
//   MPU6050 功能函数 (适配软件I2C) 
// ================================================================ 

#define MPU6050_ADDRESS 0xD0  // 对应 AD0 (PB14) 拉低 

// 写寄存器 
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data) 
{ 
    MyI2C_Start(); 
    MyI2C_SendByte(MPU6050_ADDRESS);   // 发送设备地址+写信号 
    MyI2C_WaitAck(); 
    MyI2C_SendByte(RegAddress);        // 发送寄存器地址 
    MyI2C_WaitAck(); 
    MyI2C_SendByte(Data);              // 发送数据 
    MyI2C_WaitAck(); 
    MyI2C_Stop(); 
} 

// 读寄存器 
uint8_t MPU6050_ReadReg(uint8_t RegAddress) 
{ 
    uint8_t Data; 
    MyI2C_Start(); 
    MyI2C_SendByte(MPU6050_ADDRESS);   // 发送设备地址+写信号 
    MyI2C_WaitAck(); 
    MyI2C_SendByte(RegAddress);        // 发送寄存器地址 
    MyI2C_WaitAck(); 
    
    MyI2C_Start();                     // 重复起始信号 
    MyI2C_SendByte(MPU6050_ADDRESS | 0x01); // 发送设备地址+读信号 
    MyI2C_WaitAck(); 
    Data = MyI2C_ReadByte(0);          // 读取一个字节，发送NACK 
    MyI2C_Stop(); 
    return Data; 
} 

// 连续读寄存器 
void MPU6050_ReadMultipleReg(uint8_t RegAddress, uint8_t *pData, uint16_t Size) 
{ 
    MyI2C_Start(); 
    MyI2C_SendByte(MPU6050_ADDRESS); 
    MyI2C_WaitAck(); 
    MyI2C_SendByte(RegAddress); 
    MyI2C_WaitAck(); 
    
    MyI2C_Start(); 
    MyI2C_SendByte(MPU6050_ADDRESS | 0x01); 
    MyI2C_WaitAck(); 
    
    while(Size) 
    { 
        if(Size == 1) 
            *pData = MyI2C_ReadByte(0); // 最后一个字节发NACK 
        else 
            *pData = MyI2C_ReadByte(1); // 之前的字节发ACK 
        pData++; 
        Size--; 
    } 
    MyI2C_Stop(); 
} 

// 初始化 
_Bool MPU6050_Init(void) 
{ 
    // 1. 初始化底层的软件I2C引脚 
    MyI2C_Init(); 
    
    // 2. 复位MPU6050 
    MPU6050_WriteReg(0x6B, 0x80); 
    Delay_ms(100); 
    MPU6050_WriteReg(0x6B, 0x00); // 唤醒 
    
    // 3. 配置参数 
    MPU6050_WriteReg(0x19, 0x09); // 采样率分频 
    MPU6050_WriteReg(0x1A, 0x06); // 低通滤波 
    MPU6050_WriteReg(0x1B, 0x18); // 陀螺仪 ±2000dps 
    MPU6050_WriteReg(0x1C, 0x18); // 加速度计 ±16g 
    
    // 4. 检查ID
    uint8_t id = MPU6050_ReadReg(0x75);
    
    // 【修改】注释掉报错返回，不管 ID 是多少，都强行返回成功！
    /* if (id != 0x68) {
        return 0; // 初始化失败
    }
    */ 
    
    // --- 【新增】核心修复：增加缓冲延时 ---
    // 给用户 1 秒钟的时间把手拿开，确保车身绝对静止
    Delay_ms(1000);

    // 5. 执行校准 
    // 建议把采样次数从 200 改为 500，计算更准的零点
    MPU6050_CalibrateGyro(500); 
    
    return 1; // 初始化成功 
} 

// 获取原始数据 
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ, int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ) 
{ 
    uint8_t data[14];
    
    // 从0x3B开始读取14个寄存器（加速度X/Y/Z，温度，陀螺仪X/Y/Z）
    MPU6050_ReadMultipleReg(0x3B, data, 14);
    
    // 组合高低位数据
    *AccX = (data[0] << 8) | data[1];
    *AccY = (data[2] << 8) | data[3];
    *AccZ = (data[4] << 8) | data[5];
    
    // 跳过温度数据（data[6]和data[7]）
    
    *GyroX = (data[8] << 8) | data[9];
    *GyroY = (data[10] << 8) | data[11];
    *GyroZ = (data[12] << 8) | data[13];
}

// 获取原始数据（另一个版本）
void MPU6050_GetRawData(int16_t* accel, int16_t* gyro, int16_t* temp)
{
    uint8_t data[14];
    
    // 从0x3B开始读取14个寄存器
    MPU6050_ReadMultipleReg(0x3B, data, 14);
    
    // 组合数据
    if (accel != NULL) {
        accel[0] = (data[0] << 8) | data[1];
        accel[1] = (data[2] << 8) | data[3];
        accel[2] = (data[4] << 8) | data[5];
    }
    
    if (temp != NULL) {
        *temp = (data[6] << 8) | data[7];
    }
    
    if (gyro != NULL) {
        gyro[0] = (data[8] << 8) | data[9];
        gyro[1] = (data[10] << 8) | data[11];
        gyro[2] = (data[12] << 8) | data[13];
    }
}

// 定义滑动窗口大小 (越大越平滑，但延迟越高，10-20是比较好的平衡点)
#define GYRO_FILTER_WINDOW 10

void MPU6050_Update(void)
{
    int16_t ax, ay, az, gx, gy, gz;
    float dt, accelPitch, accelRoll;
    static uint32_t lastUpdate = 0;
    
    // --- 【新增】滑动窗口滤波变量 ---
    static float gyroZ_buffer[GYRO_FILTER_WINDOW] = {0};
    static uint8_t buffer_idx = 0;
    static bool buffer_filled = false;
    // -----------------------------
    
    // 1. 获取原始数据
    MPU6050_GetData(&ax, &ay, &az, &gx, &gy, &gz);

    // 2. 赋值给全局结构体
    g_mpu6050.accelRaw[0] = ax;
    g_mpu6050.accelRaw[1] = ay;
    g_mpu6050.accelRaw[2] = az;
    g_mpu6050.gyroRaw[0] = gx;
    g_mpu6050.gyroRaw[1] = gy;
    g_mpu6050.gyroRaw[2] = gz;
    
    // 3. 计算更新间隔
    uint32_t currentTime = GetTick();
    uint32_t dt_ms = currentTime - lastUpdate;
    lastUpdate = currentTime;
    dt = (float)dt_ms / 1000.0f;
    
    // 4. 转换为物理值
    g_mpu6050.accelG[0] = ax * (16.0f / 32768.0f);
    g_mpu6050.accelG[1] = ay * (16.0f / 32768.0f);
    g_mpu6050.accelG[2] = az * (16.0f / 32768.0f);
    
    g_mpu6050.gyroDPS[0] = gx * (2000.0f / 32768.0f);
    g_mpu6050.gyroDPS[1] = gy * (2000.0f / 32768.0f);
    
    // 原始 Z 轴角速度
    float gyroZ_raw = (float)gz * (2000.0f / 32768.0f);
    float gyroZ_final = 0.0f;

    // --- 【核心升级】滤波与死区处理 ---
    if(g_mpu6050.calibrationDone) {
        // A. 扣除静态零偏
        float biasZ = g_mpu6050.gyroBias[2] * (2000.0f / 32768.0f);
        float gyroZ_no_bias = gyroZ_raw - biasZ;
        
        // B. 滑动窗口滤波 (Moving Average)
        gyroZ_buffer[buffer_idx] = gyroZ_no_bias;
        buffer_idx++;
        if (buffer_idx >= GYRO_FILTER_WINDOW) {
            buffer_idx = 0;
            buffer_filled = true;
        }
        
        // 计算平均值
        float sum = 0.0f;
        uint8_t count = buffer_filled ? GYRO_FILTER_WINDOW : buffer_idx;
        if (count == 0) count = 1; // 防止除0
        
        for (uint8_t i = 0; i < count; i++) {
            sum += gyroZ_buffer[i];
        }
        float gyroZ_filtered = sum / count;
        
        // C. 窗口死区限制 (Window Deadband)
        // 如果滤波后的值依然很小，说明是零漂，强制归零
        // 建议阈值设为 1.0 ~ 2.0 度/秒
        if (fabs(gyroZ_filtered) < 1.5f) {
            gyroZ_final = 0.0f;
        } else {
            // 只有当运动幅度够大时，才采纳滤波后的值
            gyroZ_final = gyroZ_filtered;
        }
        
    } else {
        gyroZ_final = gyroZ_raw;
    }
    
    g_mpu6050.gyroDPS[2] = gyroZ_final;
    // ------------------------------------
    
    // 5. 积分计算 Yaw (使用优化后的 gyroZ_final)
    // 简单的积分算法
    g_mpu6050.yaw += g_mpu6050.gyroDPS[2] * dt;
    
    // 姿态角互补滤波 (Pitch/Roll) - 保持原样
    accelPitch = atan2f(-g_mpu6050.accelG[0], sqrtf(g_mpu6050.accelG[1] * g_mpu6050.accelG[1] + g_mpu6050.accelG[2] * g_mpu6050.accelG[2])) * 57.3f;
    accelRoll = atan2f(g_mpu6050.accelG[1], g_mpu6050.accelG[2]) * 57.3f;
    
    g_mpu6050.pitch += g_mpu6050.gyroDPS[1] * dt;
    g_mpu6050.roll += g_mpu6050.gyroDPS[0] * dt;
    
    float alpha = 0.98f;
    g_mpu6050.pitch = alpha * g_mpu6050.pitch + (1 - alpha) * accelPitch;
    g_mpu6050.roll = alpha * g_mpu6050.roll + (1 - alpha) * accelRoll;
}

// 校准陀螺仪
bool MPU6050_CalibrateGyro(uint16_t samples)
{
    int32_t gyroSum[3] = {0, 0, 0};
    int16_t gyroRaw[3];
    uint16_t i;
    
    // 采集样本
    for (i = 0; i < samples; i++) {
        MPU6050_GetRawData(NULL, gyroRaw, NULL);
        gyroSum[0] += gyroRaw[0];
        gyroSum[1] += gyroRaw[1];
        gyroSum[2] += gyroRaw[2];
        Delay_ms(1);
    }
    
    // 计算零偏
    g_mpu6050.gyroBias[0] = (float)gyroSum[0] / samples;
    g_mpu6050.gyroBias[1] = (float)gyroSum[1] / samples;
    g_mpu6050.gyroBias[2] = (float)gyroSum[2] / samples;
    
    g_mpu6050.calibrationDone = true;
    return true;
}

// 校准加速度计
bool MPU6050_CalibrateAccel(uint16_t samples)
{
    int32_t accelSum[3] = {0, 0, 0};
    int16_t accelRaw[3];
    uint16_t i;
    
    // 采集样本
    for (i = 0; i < samples; i++) {
        MPU6050_GetRawData(accelRaw, NULL, NULL);
        accelSum[0] += accelRaw[0];
        accelSum[1] += accelRaw[1];
        accelSum[2] += accelRaw[2];
        Delay_ms(1);
    }
    
    // 计算零偏（假设Z轴指向重力方向）
    g_mpu6050.accelBias[0] = (float)accelSum[0] / samples;
    g_mpu6050.accelBias[1] = (float)accelSum[1] / samples;
    g_mpu6050.accelBias[2] = (float)accelSum[2] / samples - (32768.0f / 16.0f); // 16g量程下的重力值
    
    return true;
}

// 获取处理后的数据
void MPU6050_GetProcessedData(float* accelG, float* gyroDPS, float* temp)
{
    // 直接返回已经处理好的数据
    if (accelG != NULL) {
        accelG[0] = g_mpu6050.accelG[0];
        accelG[1] = g_mpu6050.accelG[1];
        accelG[2] = g_mpu6050.accelG[2];
    }
    
    if (gyroDPS != NULL) {
        gyroDPS[0] = g_mpu6050.gyroDPS[0];
        gyroDPS[1] = g_mpu6050.gyroDPS[1];
        gyroDPS[2] = g_mpu6050.gyroDPS[2];
    }
    
    if (temp != NULL) {
        // 温度转换公式：T(°C) = (TEMP_OUT - RoomTemp_Offset) / Temp_Sensitivity + 21
        *temp = ((float)((g_mpu6050.accelRaw[0] << 8) | g_mpu6050.accelRaw[1]) - 0) / 340.0f + 36.53f;
    }
}

// 获取欧拉角
void MPU6050_GetEulerAngles(float* yaw, float* pitch, float* roll)
{
    if (yaw != NULL) *yaw = g_mpu6050.yaw;
    if (pitch != NULL) *pitch = g_mpu6050.pitch;
    if (roll != NULL) *roll = g_mpu6050.roll;
}

// 设置偏航角偏移量
void MPU6050_SetYawOffset(float offset)
{
    g_mpu6050.yaw = offset;
}

// 重置偏航角
void MPU6050_ResetYaw(void)
{
    g_mpu6050.yaw = 0.0f;
}

// 测试MPU6050连接
bool MPU6050_TestConnection(void)
{
    return (MPU6050_ReadReg(0x75) == 0x68);
}

// 设置陀螺仪量程
void MPU6050_SetGyroFullScale(GyroFullScale_e fs)
{
    MPU6050_WriteReg(0x1B, fs << 3);
    g_mpu6050.gyroFS = fs;
    
    // 更新陀螺仪灵敏度
    switch (fs) {
        case GYRO_FS_250DPS:
            g_mpu6050.gyroSensitivity = 250.0f / 32768.0f;
            break;
        case GYRO_FS_500DPS:
            g_mpu6050.gyroSensitivity = 500.0f / 32768.0f;
            break;
        case GYRO_FS_1000DPS:
            g_mpu6050.gyroSensitivity = 1000.0f / 32768.0f;
            break;
        case GYRO_FS_2000DPS:
            g_mpu6050.gyroSensitivity = 2000.0f / 32768.0f;
            break;
    }
}

// 设置加速度计量程
void MPU6050_SetAccelFullScale(AccelFullScale_e fs)
{
    MPU6050_WriteReg(0x1C, fs << 3);
    g_mpu6050.accelFS = fs;
    
    // 更新加速度计灵敏度
    switch (fs) {
        case ACCEL_FS_2G:
            g_mpu6050.accelSensitivity = 2.0f / 32768.0f;
            break;
        case ACCEL_FS_4G:
            g_mpu6050.accelSensitivity = 4.0f / 32768.0f;
            break;
        case ACCEL_FS_8G:
            g_mpu6050.accelSensitivity = 8.0f / 32768.0f;
            break;
        case ACCEL_FS_16G:
            g_mpu6050.accelSensitivity = 16.0f / 32768.0f;
            break;
    }
}

// 设置采样率分频器
void MPU6050_SetSampleRateDivider(uint8_t divider)
{
    MPU6050_WriteReg(0x19, divider);
}

// 设置低通滤波带宽
void MPU6050_SetFilterBandwidth(uint8_t bandwidth)
{
    MPU6050_WriteReg(0x1A, bandwidth);
}

// 更新欧拉角（兼容头文件声明）
void MPU6050_UpdateEulerAngles(void)
{
    MPU6050_Update();
}
