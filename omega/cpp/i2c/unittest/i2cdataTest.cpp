#include "gtest/gtest.h"
#include "i2cdata.h"

class I2cDataTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        memset(buffer,0,sizeof(buffer));
        fillBuffer();
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test
        // (right before the destructor).
    }

    void fillBuffer(){
        uint8_t* ptr = &buffer[0];
        float temp = 0;
        *ptr = 0;//address
        ptr++;
        temp = 1;
        memcpy(ptr,&temp,sizeof(temp));
        ptr+=sizeof(temp);
        temp = 2;
        memcpy(ptr,&temp,sizeof(temp));
        ptr+=sizeof(temp);
        temp = 3;
        memcpy(ptr,&temp,sizeof(temp));
        ptr+=sizeof(temp);
        temp = 4;
        memcpy(ptr,&temp,sizeof(temp));
        ptr+=sizeof(temp);
        temp = 5;
        memcpy(ptr,&temp,sizeof(temp));
        ptr+=sizeof(temp);
        temp = (float)(1+2+3+4+5)/5;
        memcpy(ptr,&temp,sizeof(temp));    
    }
    
    i2cd::I2cData dataSet;
    uint8_t buffer[25];
};

TEST_F(I2cDataTest, setData){
    // simple valid data set 
    EXPECT_TRUE(dataSet.setData(buffer, 25));
    // Invalid params
    EXPECT_FALSE(dataSet.setData(buffer, I2C_MIN_SZ-1));
    EXPECT_FALSE(dataSet.setData(buffer, I2C_BUFFER_SIZE+1));
    EXPECT_FALSE(dataSet.setData(NULL, I2C_BUFFER_SIZE+1));
    // invalid data checksum won't match
    EXPECT_FALSE(dataSet.setData(buffer, 20));
}