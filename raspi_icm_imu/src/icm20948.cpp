#include "icm20948.h"
#include <ros/ros.h>
#include <unistd.h>

/**
 * @brief icm20948::icm20948 Constructor
 */
icm20948::icm20948()
    : mICMCurrentState(CommunicationState::UNINITIALIZED)
    , mI2cDeviceFileDescriptor(0)
    , mCurrentGyroScale(GyroScale::DPS_250)
    , mCurrentAccelScale(AccelScale::ACCEL_2G)
{
}

/**
 * @brief icm20948::initialize Initialize the communications and setup ICM20948 IMU.
 * This function should be called for the device communcations to start.
 */
const icm20948::CommunicationState& icm20948::initialize()
{
    // This would be the correct call. There is another call wiringPiSetupGpio() which assumes that
    // we would be using the Broadcom numbering scheme and doesnt not work with wiringpi pinouts.
    // More info can be found here : https://projects.drogon.net/raspberry-pi/wiringpi/functions/
    wiringPiSetup();
    // Pulldown the Pin22
    pinMode(22, INPUT);
    pullUpDnControl(22, PUD_UP);
    int interruptResult = wiringPiISR(22, INT_EDGE_RISING, &icm20948::handleInterrupt, this);
    ROS_INFO_STREAM("The result of interrupt attachment : " << interruptResult);
    // Initialize the communication with the device.
    // TODO: Are we sure that it would be successful in only one attempt?
    mI2cDeviceFileDescriptor = wiringPiI2CSetup(ICM20948REG::I2C_ICM_ADDRESS);
    // If the returned ID is negative, then there is an error.
    if(mI2cDeviceFileDescriptor < 0)
    {
        mICMCurrentState = CommunicationState::DEVICE_ERROR;
        return mICMCurrentState;
    }
    else
    {
        mICMCurrentState = CommunicationState::COMMUNICATION_SUCCESSFUL;
        // Select the userbank 0
        wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_REG_BANK_SEL,
                             ICM20948REG::REG_BANK_SEL::SELECT_USER_BANK_0);
        // Reset the device
        wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_PWR_MGMT_1,
                             ICM20948REG::ICM_PWR_MGMT_1::DEVICE_RESET);
        usleep(10000);
        // We start with PWR_MGMT_1 register which will switch on the device.
        wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_PWR_MGMT_1,
                             ICM20948REG::ICM_PWR_MGMT_1::CLKSEL_AUTO);
        // First check whether the IMU is connected.
        // Can be checked by reading the WHO_AM_I register.
        int whoAmIReturn =
            wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_WHO_AM_I);
        ROS_INFO_STREAM("[icm20948::initialize] The WHO_AM_I register value is : " << whoAmIReturn);
        if(whoAmIReturn == ICM20948REG::I2C_ICM_WHO_AM_I_ANSWER)
        {
            // Set the current state of the device.
            mICMCurrentState = CommunicationState::I2C_DEVICE_FOUND;
            // We will start initializing the device by setting the parameters that we need most.
            // Select the user bank 2 for further configuration
            wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_REG_BANK_SEL,
                                 ICM20948REG::REG_BANK_SEL::SELECT_USER_BANK_2);
            int userbankSelected =
                wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_REG_BANK_SEL);
            ROS_INFO_STREAM(
                "[icm20948::initialize] The current userbank selected is : " << userbankSelected);

            // GYRO_SMPLRT_DIV
            wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_SMPLRT_DIV,
                                 ICM20948REG::GYRO_SMPLRT_DIV::GYRO_SMPLRT_DIV_225HZ);
            // GYRO_CONFIG_1
            wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_CONFIG_1,
                                 ICM20948REG::GYRO_CONFIG_1::GYRO_DLPFCFG_361BW_376NBW |
                                     ICM20948REG::GYRO_CONFIG_1::GYRO_FS_SEL_250 |
                                     ICM20948REG::GYRO_CONFIG_1::GYRO_ENABLE_DLPF);

            // ACCEL_SMPLRT_DIV_2
            wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_SMPLRT_DIV_2,
                                 ICM20948REG::ACCEL_SMPLRT_DIV2::ACCEL_SMPLRT_DIV2_225HZ);
            // ACCEL_CONFIG
            wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_CONFIG,
                                 ICM20948REG::ACCEL_CONFIG::ACCEL_DLPFCFG_473BW_499NBW |
                                     ICM20948REG::ACCEL_CONFIG::ACCEL_FS_SEL_2G |
                                     ICM20948REG::ACCEL_CONFIG::ACCEL_ENABLE_DLPF);

            // Switch to userbank 0
            wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_REG_BANK_SEL,
                                 ICM20948REG::REG_BANK_SEL::SELECT_USER_BANK_0);
            // Enable DATA_RDY interrupt
            wiringPiI2CWriteReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_INT_ENABLE_1,
                                 ICM20948REG::INT_ENABLE_1::RAW_DATA_0_RDY_EN);
            // Interrupt pin
            int interruptEnable1Reg =
                wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_INT_ENABLE_1);
            ROS_INFO_STREAM("[icm20948::initialize] The register value of I2C_ICM_INT_ENABLE is == "
                            << interruptEnable1Reg);
        }
    }
}

/**
 * @brief icm20948::probeDevice Probe the device to find if the device is still available.0
 */
void icm20948::probeDevice()
{
    bool canBeProbed = ((mICMCurrentState == CommunicationState::COMMUNICATION_SUCCESSFUL) ||
                        (mICMCurrentState == CommunicationState::I2C_DEVICE_FOUND));
    if(canBeProbed)
    {
        int whoAmIReturn =
            wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_WHO_AM_I);
        // ROS_INFO_STREAM("[icm20948::proveDevice] The return from the device is : " <<
        // whoAmIReturn);
        if(whoAmIReturn == ICM20948REG::I2C_ICM_WHO_AM_I_ANSWER)
        {
            mICMCurrentState = CommunicationState::I2C_DEVICE_FOUND;
        }
    }
    else
    {
        mICMCurrentState = CommunicationState::COMMUNICATION_SUCCESSFUL;
    }
}

/**
 * @brief icm20948::getCommunicationState Returns the current communication state of the device.
 * @return Returns the current communication state with the imu sensor.
 */
const icm20948::CommunicationState& icm20948::getCommunicationState() const
{
    return mICMCurrentState;
}

/**
 * @brief icm20948::handleInterrupt Static function that handles the interrupt.
 * @param imuPointer The `this` pointer of this class, so that we can call other member functions of
 * this class.
 */
void icm20948::handleInterrupt(void* imuPointer)
{
    icm20948* thisPointer = reinterpret_cast<icm20948*>(imuPointer);
    thisPointer->acquireImuReadings();
}

/**
 * @brief icm20948::acquireImuReadings This will acquire IMU readings from the connected I2C
 * device(in this case its ICM20948 sensor).
 */
void icm20948::acquireImuReadings()
{
    // First we create a buffer to hold the values.
    // We will be reading 1 byte everytime but the actual resolution is 16 bits. So to hold all the
    // 6 bytes, we will need a buffer.
    std::array<uint8_t, 2> lowHighAccelBuffer = {0, 0};
    std::array<int16_t, 3> rawAccelBuffer	 = {0, 0, 0};
    // ACCEL_X
    lowHighAccelBuffer[0] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_XOUT_H);
    lowHighAccelBuffer[1] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_XOUT_L);
    rawAccelBuffer[0] = (lowHighAccelBuffer[0] << 8) | lowHighAccelBuffer[1];
    // ACCEL_Y
    lowHighAccelBuffer[0] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_YOUT_H);
    lowHighAccelBuffer[1] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_YOUT_L);
    rawAccelBuffer[1] = (lowHighAccelBuffer[0] << 8) | lowHighAccelBuffer[1];
    // ACCEL_Z
    lowHighAccelBuffer[0] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_ZOUT_H);
    lowHighAccelBuffer[1] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_ACCEL_ZOUT_L);
    rawAccelBuffer[2] = (lowHighAccelBuffer[0] << 8) | lowHighAccelBuffer[1];
    // Now we convert the values into m/sec2
    mCurrentImuMessage.linear_acceleration.x =
        (rawAccelBuffer[0] / ACCEL_2G) * icm20948::ACCEL_DUE_TO_GRAVITY;
    mCurrentImuMessage.linear_acceleration.y =
        (rawAccelBuffer[1] / ACCEL_2G) * icm20948::ACCEL_DUE_TO_GRAVITY;
    mCurrentImuMessage.linear_acceleration.z =
        (rawAccelBuffer[2] / ACCEL_2G) * icm20948::ACCEL_DUE_TO_GRAVITY;
    // Gyroscope messages
    std::array<uint8_t, 2> lowHighGyroBuffer = {0, 0};
    std::array<int16_t, 3> rawGyroBuffer	 = {0, 0, 0};
    // // GYRO_X
    lowHighGyroBuffer[0] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_XOUT_H);
    lowHighAccelBuffer[1] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_XOUT_L);
    rawGyroBuffer[0] = (lowHighGyroBuffer[0] << 8) | lowHighGyroBuffer[1];
    // // GYRO_Y
    lowHighGyroBuffer[0] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_YOUT_H);
    lowHighAccelBuffer[1] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_YOUT_L);
    rawGyroBuffer[1] = (lowHighGyroBuffer[0] << 8) | lowHighGyroBuffer[1];
    // // GYRO_Z
    lowHighGyroBuffer[0] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_ZOUT_H);
    lowHighAccelBuffer[1] =
        wiringPiI2CReadReg8(mI2cDeviceFileDescriptor, ICM20948REG::I2C_ICM_GYRO_ZOUT_L);
    rawGyroBuffer[2] = (lowHighGyroBuffer[0] << 8) | lowHighGyroBuffer[1];
    // Set the values in the IMU message
    mCurrentImuMessage.angular_velocity.x =
        (rawGyroBuffer[0] / GYRO_DPS_250) * DEGREES_TO_RAD_FACTOR;
    mCurrentImuMessage.angular_velocity.y =
        (rawGyroBuffer[1] / GYRO_DPS_250) * DEGREES_TO_RAD_FACTOR;
    mCurrentImuMessage.angular_velocity.z =
        (rawGyroBuffer[2] / GYRO_DPS_250) * DEGREES_TO_RAD_FACTOR;
    mCurrentImuMessage.header.stamp = ros::Time::now();
    mSetImuValueFunction(mCurrentImuMessage);
}

/**
 * @brief icm20948::setImuValueFunction The function that will be called when the interrupt is
 * called from the IMU sensor.
 * @param imuValueFunction The function to be called when interrupt happens.
 */
void icm20948::setImuValueFunction(SetImuValues& imuValueFunction)
{
    mSetImuValueFunction = imuValueFunction;
}
