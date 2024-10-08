#if defined(ARDUINO) && ARDUINO >= 100  // Arduino IDE Version
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "RobyServo.h"

// Macros /////////////////////////////////////////////////////////////////////

#define sendData(packet, length)  	(varSerial->write(packet, length))    	// Write Over Serial
#define serialFlush()				(varSerial->flush())					// Wait until buffer empty
#define availableData() 			(varSerial->available())    			// Check Serial Data Available
#define readData()      			(varSerial->read())         			// Read Serial Data
#define peekData()      			(varSerial->peek())         			// Peek Serial Data
#define beginCom(args)  			(varSerial->begin(args))   				// Begin Serial Comunication
#define endCom()        			(varSerial->end())          			// End Serial Comunication

#define setDPin(DirPin,Mode)   		(pinMode(DirPin,Mode))       			// Select the Switch to TX/RX Mode Pin
#define switchCom(DirPin,Mode) 		(digitalWrite(DirPin,Mode))  			// Switch to TX/RX Mode

#define delayus(args) 				(delayMicroseconds(args))  				// Delay Microseconds

// Private Methods ////////////////////////////////////////////////////////////
void RobyServo::sendSyncData(const uint8_t data)
{
	if(varSerial)
	{
		varSerial->write(data);
		varSerial->flush();
	}
}

int RobyServo::dataSizePerServoSynch()
{
	if(use_speed_synch)
		return AX12_SYNCH_PER_SERVO_WSPEED;
	else
		return AX12_SYNCH_PER_SERVO_WOSPEED;
}

void RobyServo::printSyncData(int length, unsigned char checksum)
{
	int data_size_per_servo = dataSizePerServoSynch();
    int Checksum = BROADCAST_ID + length + AX_SYNC_WRITE + AX_GOAL_POSITION_L + (data_size_per_servo-1);
	
    
	Serial.print(0xFF, HEX);  
	Serial.print(", ");

	Serial.print(0xFF, HEX);  
	Serial.print(", ");

	Serial.print(BROADCAST_ID, HEX);  
	Serial.print(", ");

	Serial.print(length, HEX);  
	Serial.print(", ");

	Serial.print(AX_SYNC_WRITE, HEX);  
	Serial.print(", ");

	Serial.print(AX_GOAL_POSITION_L, HEX);  
	Serial.print(", ");

	Serial.print(data_size_per_servo-1, HEX);  
	Serial.print(", ");
	
	int dataidx=0, id=0, temp=0;
    for(int servo=0; servo<total_sync_servos; servo++)
    {
		//Now write the data for the servo
		for(int servo_data_idx=0; servo_data_idx<data_size_per_servo; servo_data_idx++)
		{
			temp = sync_data[dataidx];
			Serial.print(temp, HEX);  
			Serial.print(", ");
			Checksum += temp;
			dataidx++;
		}
    } 
	unsigned char sum = (0xff - (Checksum % 256));
    
	Serial.print(checksum, HEX);  
	//Serial.print(sum, HEX);  
	Serial.print("\n");
}

void RobyServo::startSyncWrite(bool use_speed)
{
	total_sync_servos = 0;
	use_speed_synch = use_speed;
}

int RobyServo::getLowByte(int val) 
{
	return (val & 0xff);
};

int RobyServo::getHighByte(int val) 
{
	return ((val & 0xff00)>> 8);
};

void RobyServo::addServoToSync(int id, int goal_pos, int goal_speed)
{
	int data_size_per_servo = dataSizePerServoSynch();

	int idx = total_sync_servos*data_size_per_servo;

	sync_data[idx] = id;
	sync_data[idx+1] = getLowByte(goal_pos);
	sync_data[idx+2] = getHighByte(goal_pos);

	if(use_speed_synch) {
		sync_data[idx+3] = getLowByte(goal_speed);
		sync_data[idx+4] = getHighByte(goal_speed);
	}
	
	//Serial.print("servos: ");  
	//Serial.print(total_sync_servos);  
	//Serial.print(" idx: ");  
	//Serial.print(idx);  
	//Serial.print(" id: ");  
	//Serial.print(id);  
	//Serial.print(" pos: ");  
	//Serial.print(goal_pos); 
	//Serial.print(" speed: ");  
	//Serial.print(goal_speed); 
	//Serial.print("\n");
		
	//for(int i=0; i<(idx+data_size_per_servo); i++)
	//{
	//	Serial.print(sync_data[i], HEX);  
	//	Serial.print(", ");
	//}
	//Serial.println("\n");

    total_sync_servos++;	
}

int RobyServo::read_error(void)
{
	
	Time_Counter = 0;
	while((availableData() < 5) & (Time_Counter < TIME_OUT)) // Wait for Data
	{
		Time_Counter++;
		delayus(1000);
	}
	
	while (availableData() > 0)
	{
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                                    // Start Bytes
			readData();                                    // Ax-12 ID
			readData();                                    // Length
			Error_Byte = readData();                       // Error
				return (Error_Byte);
		}
	}
	return (-1);											 // No Ax Response
}

// Public Methods /////////////////////////////////////////////////////////////

void RobyServo::begin(long baud, unsigned char directionPin, HardwareSerial *srl)
{
	varSerial = srl;
	Direction_Pin = directionPin;
	setDPin(Direction_Pin, OUTPUT);
	beginCom(baud);
}

void RobyServo::end()
{
	endCom();
}

int RobyServo::reset(unsigned char ID)
{
	const unsigned int length = 6;
	unsigned char packet[length];
	
	Checksum = (~(ID + AX_RESET_LENGTH + AX_RESET)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_RESET_LENGTH;
	packet[4] = AX_RESET;
	packet[5] = Checksum;

    return (sendAXPacket(packet, length));
}

int RobyServo::ping(unsigned char ID)
{
	const unsigned int length = 6;
	unsigned char packet[length];

	Checksum = (~(ID + AX_READ_DATA + AX_PING)) & 0xFF;
	
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_READ_DATA;
	packet[4] = AX_PING;
	packet[5] = Checksum;

	return (sendAXPacket(packet, length));
}

int RobyServo::setID(unsigned char ID, unsigned char newID)
{    
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_ID_LENGTH + AX_WRITE_DATA + AX_ID + newID)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_ID_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_ID;
	packet[6] = newID;
	packet[7] = Checksum;

    return (sendAXPacket(packet, length));
}

int RobyServo::setBD(unsigned char ID, long baud)
{    
	const unsigned int length = 8;
	unsigned char packet[length];

	unsigned char Baud_Rate = (2000000/baud) - 1;
	Checksum = (~(ID + AX_BD_LENGTH + AX_WRITE_DATA + AX_BAUD_RATE + Baud_Rate)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_BD_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_BAUD_RATE;
	packet[6] = Baud_Rate;
	packet[7] = Checksum;
    
	return (sendAXPacket(packet, length));
}

int RobyServo::move(unsigned char ID, int Position)
{
    char Position_H,Position_L;
    Position_H = Position >> 8;           // 16 bits - 2 x 8 bits variables
    Position_L = Position;

    const unsigned int length = 9;
    unsigned char packet[length];

	Checksum = (~(ID + AX_GOAL_LENGTH + AX_WRITE_DATA + AX_GOAL_POSITION_L + Position_L + Position_H)) & 0xFF;

    packet[0] = AX_START;
    packet[1] = AX_START;
    packet[2] = ID;
    packet[3] = AX_GOAL_LENGTH;
    packet[4] = AX_WRITE_DATA;
    packet[5] = AX_GOAL_POSITION_L;
    packet[6] = Position_L;
    packet[7] = Position_H;
    packet[8] = Checksum;

    return (sendAXPacket(packet, length));
}

int RobyServo::moveSpeed(unsigned char ID, int Position, int Speed)
{
    char Position_H,Position_L,Speed_H,Speed_L;
    Position_H = Position >> 8;    
    Position_L = Position;                // 16 bits - 2 x 8 bits variables
    Speed_H = Speed >> 8;
    Speed_L = Speed;                      // 16 bits - 2 x 8 bits variables

    const unsigned int length = 11;
    unsigned char packet[length];
    
    Checksum = (~(ID + AX_GOAL_SP_LENGTH + AX_WRITE_DATA + AX_GOAL_POSITION_L + Position_L + Position_H + Speed_L + Speed_H)) & 0xFF;

    packet[0] = AX_START;
    packet[1] = AX_START;
    packet[2] = ID;
    packet[3] = AX_GOAL_SP_LENGTH;
    packet[4] = AX_WRITE_DATA;
    packet[5] = AX_GOAL_POSITION_L;
    packet[6] = Position_L;
    packet[7] = Position_H;
    packet[8] = Speed_L;
    packet[9] = Speed_H;
    packet[10] = Checksum;

    return (sendAXPacket(packet, length));
}

int RobyServo::setEndless(unsigned char ID, bool Status)
{
	if ( Status )
	{
		const unsigned int length = 9;
		unsigned char packet[length];

		Checksum = (~(ID + AX_GOAL_LENGTH + AX_WRITE_DATA + AX_CCW_ANGLE_LIMIT_L)) & 0xFF;

	    packet[0] = AX_START;
	    packet[1] = AX_START;
	    packet[2] = ID;
	    packet[3] = AX_GOAL_LENGTH;
	    packet[4] = AX_WRITE_DATA;
	    packet[5] = AX_CCW_ANGLE_LIMIT_L;
	    packet[6] = 0; 						// full rotation
	    packet[7] = 0;						// full rotation
	    packet[8] = Checksum;

	    return (sendAXPacket(packet, length));
	}
	else
	{
		turn(ID,0,0);

		const unsigned int length = 9;
		unsigned char packet[length];

		Checksum = (~(ID + AX_GOAL_LENGTH + AX_WRITE_DATA + AX_CCW_ANGLE_LIMIT_L + AX_CCW_AL_L + AX_CCW_AL_H)) & 0xFF;

	    packet[0] = AX_START;
	    packet[1] = AX_START;
	    packet[2] = ID;
	    packet[3] = AX_GOAL_LENGTH;
	    packet[4] = AX_WRITE_DATA;
	    packet[5] = AX_CCW_ANGLE_LIMIT_L;
	    packet[6] = AX_CCW_AL_L;
	    packet[7] = AX_CCW_AL_H;
	    packet[8] = Checksum;

	    return (sendAXPacket(packet, length));
	}
}

int RobyServo::turn(unsigned char ID, bool SIDE, int Speed)
{		
		if (SIDE == LEFT)
		{
			char Speed_H,Speed_L;
			Speed_H = Speed >> 8;
			Speed_L = Speed;                     // 16 bits - 2 x 8 bits variables
			
			const unsigned int length = 9;
			unsigned char packet[length];
			
			Checksum = (~(ID + AX_SPEED_LENGTH + AX_WRITE_DATA + AX_GOAL_SPEED_L + Speed_L + Speed_H)) & 0xFF;

		    packet[0] = AX_START;
		    packet[1] = AX_START;
		    packet[2] = ID;
		    packet[3] = AX_SPEED_LENGTH;
		    packet[4] = AX_WRITE_DATA;
		    packet[5] = AX_GOAL_SPEED_L;
		    packet[6] = Speed_L;
		    packet[7] = Speed_H;
		    packet[8] = Checksum;

		    return (sendAXPacket(packet, length));
		}

		else
		{
			char Speed_H,Speed_L;
			Speed_H = (Speed >> 8) + 4;
			Speed_L = Speed;                     // 16 bits - 2 x 8 bits variables
			
			const unsigned int length = 9;
			unsigned char packet[length];

			Checksum = (~(ID + AX_SPEED_LENGTH + AX_WRITE_DATA + AX_GOAL_SPEED_L + Speed_L + Speed_H)) & 0xFF;
			
		    packet[0] = AX_START;
		    packet[1] = AX_START;
		    packet[2] = ID;
		    packet[3] = AX_SPEED_LENGTH;
		    packet[4] = AX_WRITE_DATA;
		    packet[5] = AX_GOAL_SPEED_L;
		    packet[6] = Speed_L;
		    packet[7] = Speed_H;
		    packet[8] = Checksum;

		    return (sendAXPacket(packet, length));
		}
}

int RobyServo::moveRW(unsigned char ID, int Position)
{
    char Position_H,Position_L;
    Position_H = Position >> 8;           // 16 bits - 2 x 8 bits variables
    Position_L = Position;

	const unsigned int length = 9;
	unsigned char packet[length];

    Checksum = (~(ID + AX_GOAL_LENGTH + AX_REG_WRITE + AX_GOAL_POSITION_L + Position_L + Position_H)) & 0xFF;

    packet[0] = AX_START;
    packet[1] = AX_START;
    packet[2] = ID;
    packet[3] = AX_GOAL_LENGTH;
    packet[4] = AX_REG_WRITE;
    packet[5] = AX_GOAL_POSITION_L;
    packet[6] = Position_L;
    packet[7] = Position_H;
    packet[8] = Checksum;
	
    return (sendAXPacket(packet, length));
}

int RobyServo::moveSpeedRW(unsigned char ID, int Position, int Speed)
{
    char Position_H,Position_L,Speed_H,Speed_L;
    Position_H = Position >> 8;    
    Position_L = Position;                // 16 bits - 2 x 8 bits variables
    Speed_H = Speed >> 8;
    Speed_L = Speed;                      // 16 bits - 2 x 8 bits variables

	const unsigned int length = 11;
	unsigned char packet[length];

    Checksum = (~(ID + AX_GOAL_SP_LENGTH + AX_REG_WRITE + AX_GOAL_POSITION_L + Position_L + Position_H + Speed_L + Speed_H)) & 0xFF;

    packet[0] = AX_START;
    packet[1] = AX_START;
    packet[2] = ID;
    packet[3] = AX_GOAL_SP_LENGTH;
    packet[4] = AX_REG_WRITE;
    packet[5] = AX_GOAL_POSITION_L;
    packet[6] = Position_L;
    packet[7] = Position_H;
    packet[8] = Speed_L;
    packet[9] = Speed_H;
    packet[10] = Checksum;
	
    return (sendAXPacket(packet, length));
}

void RobyServo::action()
{	
	const unsigned int length = 6;
	unsigned char packet[length];

    packet[0] = AX_START;
    packet[1] = AX_START;
    packet[2] = BROADCAST_ID;
    packet[3] = AX_ACTION_LENGTH;
    packet[4] = AX_ACTION;
    packet[5] = AX_ACTION_CHECKSUM;

    sendAXPacket(packet, length);
}

int RobyServo::torqueStatus( unsigned char ID, bool Status)
{
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_TORQUE_LENGTH + AX_WRITE_DATA + AX_TORQUE_ENABLE + Status)) & 0xFF;

    packet[0] = AX_START;
    packet[1] = AX_START;
    packet[2] = ID;
    packet[3] = AX_TORQUE_LENGTH;
    packet[4] = AX_WRITE_DATA;
    packet[5] = AX_TORQUE_ENABLE;
    packet[6] = Status;
    packet[7] = Checksum;
    
    return (sendAXPacket(packet, length));
}

int RobyServo::ledStatus(unsigned char ID, bool Status)
{    
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_LED_LENGTH + AX_WRITE_DATA + AX_LED + Status)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_LED_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_LED;
	packet[6] = Status;
	packet[7] = Checksum;

	return (sendAXPacket(packet, length)); // return error
}

int RobyServo::readTemperature(unsigned char ID)
{	
	const unsigned int length = 8;
	unsigned char packet[length];

    Checksum = (~(ID + AX_TEM_LENGTH + AX_READ_DATA + AX_PRESENT_TEMPERATURE + AX_BYTE_READ)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_TEM_LENGTH;
	packet[4] = AX_READ_DATA;
	packet[5] = AX_PRESENT_TEMPERATURE;
	packet[6] = AX_BYTE_READ;
	packet[7] = Checksum;

	sendAXPacketNoError(packet, length);
	
    Temperature_Byte = -1;
    Time_Counter = 0;
    while((availableData() < 6) & (Time_Counter < TIME_OUT))
    {
		Time_Counter++;
		delayus(1000);
    }
	
    while (availableData() > 0)
    {
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                            // Start Bytes
			readData();                            // Ax-12 ID
			readData();                            // Length
			if( (Error_Byte = readData()) != 0 )   // Error
				return (Error_Byte*(-1));
			Temperature_Byte = readData();         // Temperature
		}
    }
	return (Temperature_Byte);               // Returns the read temperature
}

int RobyServo::readPosition(unsigned char ID)
{	
	const unsigned int length = 8;
	unsigned char packet[length];

    Checksum = (~(ID + AX_POS_LENGTH + AX_READ_DATA + AX_PRESENT_POSITION_L + AX_BYTE_READ_POS)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_POS_LENGTH;
	packet[4] = AX_READ_DATA;
	packet[5] = AX_PRESENT_POSITION_L;
	packet[6] = AX_BYTE_READ_POS;
	packet[7] = Checksum;

	sendAXPacketNoError(packet, length);
	
    Position_Long_Byte = -1;
	Time_Counter = 0;
    while((availableData() < 7) & (Time_Counter < TIME_OUT))
    {
		Time_Counter++;
		delayus(1000);
    }
	
    while (availableData() > 0)
    {
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                            // Start Bytes
			readData();                            // Ax-12 ID
			readData();                            // Length
			if( (Error_Byte = readData()) != 0 )   // Error
				return (Error_Byte*(-1));
    
			Position_Low_Byte = readData();            // Position Bytes
			Position_High_Byte = readData();
			Position_Long_Byte = Position_High_Byte << 8; 
			Position_Long_Byte = Position_Long_Byte + Position_Low_Byte;
		}
    }
	return (Position_Long_Byte);     // Returns the read position
}

int RobyServo::readVoltage(unsigned char ID)
{    
	const unsigned int length = 8;
	unsigned char packet[length];

    Checksum = (~(ID + AX_VOLT_LENGTH + AX_READ_DATA + AX_PRESENT_VOLTAGE + AX_BYTE_READ)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_VOLT_LENGTH;
	packet[4] = AX_READ_DATA;
	packet[5] = AX_PRESENT_VOLTAGE;
	packet[6] = AX_BYTE_READ;
	packet[7] = Checksum;

    sendAXPacketNoError(packet, length);
	
    Voltage_Byte = -1;
	Time_Counter = 0;
    while((availableData() < 6) & (Time_Counter < TIME_OUT))
    {
		Time_Counter++;
		delayus(1000);
    }
	
    while (availableData() > 0)
    {
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                            // Start Bytes
			readData();                            // Ax-12 ID
			readData();                            // Length
			if( (Error_Byte = readData()) != 0 )   // Error
				return (Error_Byte*(-1));
			Voltage_Byte = readData();             // Voltage
		}
    }
	return (Voltage_Byte);               // Returns the read Voltage
}

int RobyServo::setTempLimit(unsigned char ID, unsigned char Temperature)
{
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_TL_LENGTH + AX_WRITE_DATA + AX_LIMIT_TEMPERATURE + Temperature)) & 0xFF;
	
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_TL_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_LIMIT_TEMPERATURE;
	packet[6] = Temperature;
	packet[7] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::setVoltageLimit(unsigned char ID, unsigned char DVoltage, unsigned char UVoltage)
{
	const unsigned int length = 9;
	unsigned char packet[length];

	Checksum = (~(ID + AX_VL_LENGTH + AX_WRITE_DATA + AX_DOWN_LIMIT_VOLTAGE + DVoltage + UVoltage)) & 0xFF;
	
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_VL_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_DOWN_LIMIT_VOLTAGE;
	packet[6] = DVoltage;
	packet[7] = UVoltage;
	packet[8] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::setAngleLimit(unsigned char ID, int CWLimit, int CCWLimit)
{
	char CW_H,CW_L,CCW_H,CCW_L;
    CW_H = CWLimit >> 8;    
    CW_L = CWLimit;                // 16 bits - 2 x 8 bits variables
    CCW_H = CCWLimit >> 8;
    CCW_L = CCWLimit;  

	const unsigned int length = 12;
	unsigned char packet[length];
	
	Checksum = (~(ID + AX_VL_LENGTH + AX_WRITE_DATA + AX_CW_ANGLE_LIMIT_L + CW_H + CW_L + AX_CCW_ANGLE_LIMIT_L + CCW_H + CCW_L)) & 0xFF;
	
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_CCW_CW_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_CW_ANGLE_LIMIT_L;
	packet[6] = CW_L;
	packet[7] = CW_H;
	packet[8] = AX_CCW_ANGLE_LIMIT_L;
	packet[9] = CCW_L;
	packet[10] = CCW_H;
	packet[11] = Checksum;

	return (sendAXPacket(packet, length));
}

int RobyServo::setMaxTorque(unsigned char ID, int MaxTorque)
{
    char MaxTorque_H,MaxTorque_L;
    MaxTorque_H = MaxTorque >> 8;           // 16 bits - 2 x 8 bits variables
    MaxTorque_L = MaxTorque;
    
	const unsigned int length = 9;
	unsigned char packet[length];

	Checksum = (~(ID + AX_MT_LENGTH + AX_WRITE_DATA + AX_MAX_TORQUE_L + MaxTorque_L + MaxTorque_H)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_MT_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_MAX_TORQUE_L;
	packet[6] = MaxTorque_L;
	packet[7] = MaxTorque_H;
	packet[8] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::setSRL(unsigned char ID, unsigned char SRL)
{    
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_SRL_LENGTH + AX_WRITE_DATA + AX_RETURN_LEVEL + SRL)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_SRL_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_RETURN_LEVEL;
	packet[6] = SRL;
	packet[7] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::setRDT(unsigned char ID, unsigned char RDT)
{    
	const unsigned int length = 8;
	unsigned char packet[length];
	
	Checksum = (~(ID + AX_RDT_LENGTH + AX_WRITE_DATA + AX_RETURN_DELAY_TIME + (RDT / 2))) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_RDT_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_RETURN_DELAY_TIME;
	packet[6] = (RDT/2);
	packet[7] = Checksum;
    
	return (sendAXPacket(packet, length));
}

int RobyServo::setLEDAlarm(unsigned char ID, unsigned char LEDAlarm)
{    
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_LEDALARM_LENGTH + AX_WRITE_DATA + AX_ALARM_LED + LEDAlarm)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_LEDALARM_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_ALARM_LED;
	packet[6] = LEDAlarm;
	packet[7] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::setShutdownAlarm(unsigned char ID, unsigned char SALARM)
{    
	const unsigned int length = 8;
	unsigned char packet[length];
	
	Checksum = (~(ID + AX_SALARM_LENGTH + AX_ALARM_SHUTDOWN + AX_ALARM_LED + SALARM)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_SALARM_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_ALARM_SHUTDOWN;
	packet[6] = SALARM;
	packet[7] = Checksum;
    
	return (sendAXPacket(packet, length));
}

int RobyServo::setCMargin(unsigned char ID, unsigned char CWCMargin, unsigned char CCWCMargin)
{
	const unsigned int length = 10;
	unsigned char packet[length];

	Checksum = (~(ID + AX_CM_LENGTH + AX_WRITE_DATA + AX_CW_COMPLIANCE_MARGIN + CWCMargin + AX_CCW_COMPLIANCE_MARGIN + CCWCMargin)) & 0xFF;
	
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_CM_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_CW_COMPLIANCE_MARGIN;
	packet[6] = CWCMargin;
	packet[7] = AX_CCW_COMPLIANCE_MARGIN;
	packet[8] = CCWCMargin;
	packet[9] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::setCSlope(unsigned char ID, unsigned char CWCSlope, unsigned char CCWCSlope)
{
	const unsigned int length = 10;
	unsigned char packet[length];

	Checksum = (~(ID + AX_CS_LENGTH + AX_WRITE_DATA + AX_CW_COMPLIANCE_SLOPE + CWCSlope + AX_CCW_COMPLIANCE_SLOPE + CCWCSlope)) & 0xFF;
	
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_CS_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_CW_COMPLIANCE_SLOPE;
	packet[6] = CWCSlope;
	packet[7] = AX_CCW_COMPLIANCE_SLOPE;
	packet[8] = CCWCSlope;
	packet[9] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::setPunch(unsigned char ID, int Punch)
{
    char Punch_H,Punch_L;
    Punch_H = Punch >> 8;           // 16 bits - 2 x 8 bits variables
    Punch_L = Punch;

	const unsigned int length = 9;
	unsigned char packet[length];

	Checksum = (~(ID + AX_PUNCH_LENGTH + AX_WRITE_DATA + AX_PUNCH_L + Punch_L + Punch_H)) & 0xFF;
    
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_PUNCH_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_PUNCH_L;
	packet[6] = Punch_L;
	packet[7] = Punch_H;
	packet[8] = Checksum;
	
	return (sendAXPacket(packet, length));
}

int RobyServo::moving(unsigned char ID)
{	
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_MOVING_LENGTH + AX_READ_DATA + AX_MOVING + AX_BYTE_READ)) & 0xFF;
    
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_MOVING_LENGTH;
	packet[4] = AX_READ_DATA;
	packet[5] = AX_MOVING;
	packet[6] = AX_BYTE_READ;
	packet[7] = Checksum;
	
	sendAXPacketNoError(packet, length);

    Moving_Byte = -1;
    Time_Counter = 0;
    while((availableData() < 6) & (Time_Counter < TIME_OUT))
    {
		Time_Counter++;
		delayus(1000);
    }
	
    while (availableData() > 0)
    {
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                           	// Start Bytes
			readData();                           	// Ax-12 ID
			readData();                           	// Length
			if( (Error_Byte = readData()) != 0 )   	// Error
				return (Error_Byte*(-1));
			Moving_Byte = readData();         		// Moving
		}
    }
	return (Moving_Byte);              				// Returns the read Moving
}

int RobyServo::lockRegister(unsigned char ID)
{    
	const unsigned int length = 8;
	unsigned char packet[length];
	
	Checksum = (~(ID + AX_LR_LENGTH + AX_WRITE_DATA + AX_LOCK + LOCK)) & 0xFF;
    
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_LR_LENGTH;
	packet[4] = AX_WRITE_DATA;
	packet[5] = AX_LOCK;
	packet[6] = LOCK;
	packet[7] = Checksum;

	return (sendAXPacket(packet, length));
}

int RobyServo::RWStatus(unsigned char ID)
{	
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_RWS_LENGTH + AX_READ_DATA + AX_REGISTERED_INSTRUCTION + AX_BYTE_READ)) & 0xFF;
    
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_RWS_LENGTH;
	packet[4] = AX_READ_DATA;
	packet[5] = AX_REGISTERED_INSTRUCTION;
	packet[6] = AX_BYTE_READ;
	packet[7] = Checksum;
	
	sendAXPacketNoError(packet, length);

    RWS_Byte = -1;
    Time_Counter = 0;
    while((availableData() < 6) & (Time_Counter < TIME_OUT))
    {
		Time_Counter++;
		delayus(1000);
    }
	
    while (availableData() > 0)
    {
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                            	// Start Bytes
			readData();                            	// Ax-12 ID
			readData();                            	// Length
			if( (Error_Byte = readData()) != 0 )   	// Error
				return (Error_Byte*(-1));
			RWS_Byte = readData();         			// RWStatus
		}
    }
	return (RWS_Byte);               				// Returns the read RWStatus
}

int RobyServo::readSpeed(unsigned char ID)
{	
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_POS_LENGTH + AX_READ_DATA + AX_PRESENT_SPEED_L + AX_BYTE_READ_POS)) & 0xFF;
	
	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_POS_LENGTH;
	packet[4] = AX_READ_DATA;
	packet[5] = AX_PRESENT_SPEED_L;
	packet[6] = AX_BYTE_READ_POS;
	packet[7] = Checksum;
	
	sendAXPacketNoError(packet, length);

    Speed_Long_Byte = -1;
	Time_Counter = 0;
    while((availableData() < 7) & (Time_Counter < TIME_OUT))
    {
		Time_Counter++;
		delayus(1000);
    }
	
    while (availableData() > 0)
    {
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                            // Start Bytes
			readData();                            // Ax-12 ID
			readData();                            // Length
			if( (Error_Byte = readData()) != 0 )   // Error
				return (Error_Byte*(-1));
			
			Speed_Low_Byte = readData();            // Position Bytes
			Speed_High_Byte = readData();
			Speed_Long_Byte = Speed_High_Byte << 8; 
			Speed_Long_Byte = Speed_Long_Byte + Speed_Low_Byte;
		}
    }
	return (Speed_Long_Byte);     // Returns the read position
}

int RobyServo::readLoad(unsigned char ID)
{	
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + AX_POS_LENGTH + AX_READ_DATA + AX_PRESENT_LOAD_L + AX_BYTE_READ_POS)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = AX_POS_LENGTH;
	packet[4] = AX_READ_DATA;
	packet[5] = AX_PRESENT_LOAD_L;
	packet[6] = AX_BYTE_READ_POS;
	packet[7] = Checksum;
	
	sendAXPacketNoError(packet, length);

    Load_Long_Byte = -1;
	Time_Counter = 0;
    while((availableData() < 7) & (Time_Counter < TIME_OUT))
    {
		Time_Counter++;
		delayus(1000);
    }
	
    while (availableData() > 0)
    {
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                            // Start Bytes
			readData();                            // Ax-12 ID
			readData();                            // Length
			if( (Error_Byte = readData()) != 0 )   // Error
				return (Error_Byte*(-1));
			
			Load_Low_Byte = readData();            // Position Bytes
			Load_High_Byte = readData();
			Load_Long_Byte = Load_High_Byte << 8; 
			Load_Long_Byte = Load_Long_Byte + Load_Low_Byte;
		}
    }
	return (Load_Long_Byte);     // Returns the read position
}

int RobyServo::sendAXPacket(unsigned char * packet, unsigned int length)
{
	switchCom(Direction_Pin, TX_MODE); 	// Switch to Transmission  Mode

	sendData(packet, length);			// Send data through sending buffer
	serialFlush(); 							// Wait until buffer is empty

	switchCom(Direction_Pin, RX_MODE); 	// Switch back to Reception Mode

	return (read_error());              // Return the read error
}

void RobyServo::sendAXPacketNoError(unsigned char * packet, unsigned int length)
{
	switchCom(Direction_Pin, TX_MODE); 	// Switch to Transmission  Mode

	sendData(packet, length);			// Send data through sending buffer
	serialFlush(); 							// Wait until buffer is empty

	switchCom(Direction_Pin, RX_MODE); 	// Switch back to Reception Mode
}

int RobyServo::readRegister(unsigned char ID, unsigned char reg, unsigned char reg_len)
{
	const unsigned int length = 8;
	unsigned char packet[length];

	Checksum = (~(ID + 4 + AX_READ_DATA + reg + reg_len)) & 0xFF;

	packet[0] = AX_START;
	packet[1] = AX_START;
	packet[2] = ID;
	packet[3] = 4;
	packet[4] = AX_READ_DATA;
	packet[5] = reg;
	packet[6] = reg_len;
	packet[7] = Checksum;

	sendAXPacketNoError(packet, length);

	returned_Byte = -1;
	Time_Counter = 0;
	while((availableData() < 7) & (Time_Counter < TIME_OUT))
	{
		Time_Counter++;
		delayus(1000);
	}

	while (availableData() > 0)
	{
		Incoming_Byte = readData();
		if ( (Incoming_Byte == 255) & (peekData() == 255) )
		{
			readData();                            // Start Bytes
			readData();                            // Ax-12 ID
			readData();                            // Length
			if( (Error_Byte = readData()) != 0 )   // Error
				return (Error_Byte*(-1));

			switch (reg_len)
			{
				case 1:
					returned_Byte = readData();
					break;
				case 2:
					returned_Byte = readData();
					returned_Byte += readData() << 8;
				break;
			}
		}
	}
	return (returned_Byte);     // Returns the read position
}

void RobyServo::writeSyncData(bool print_data)
{	
    int temp;
	int data_size_per_servo = dataSizePerServoSynch();

	int	length = 4 + (data_size_per_servo * total_sync_servos);		
    int Checksum = 254 + length + AX_SYNC_WRITE + AX_GOAL_POSITION_L + (data_size_per_servo-1);

	switchCom(Direction_Pin,TX_MODE);
    sendSyncData(AX_START);
    sendSyncData(AX_START);
    sendSyncData(BROADCAST_ID);
    sendSyncData(length);
    sendSyncData(AX_SYNC_WRITE);
    sendSyncData(AX_GOAL_POSITION_L);
    sendSyncData(data_size_per_servo-1);

	int dataidx=0, id=0;
    for(int servo=0; servo<total_sync_servos; servo++)
    {
		//Now write the data for the servo
		for(int servo_data_idx=0; servo_data_idx<data_size_per_servo; servo_data_idx++)
		{
			temp = sync_data[dataidx];
			Checksum += temp;
			sendSyncData(temp);
			dataidx++;
		}
    } 

	unsigned char sum = (0xff - (Checksum % 256));
    sendSyncData(sum);
    delayus(TX_DELAY_TIME);
	switchCom(Direction_Pin,RX_MODE);
	
	if(print_data)
		printSyncData(length, sum);
	
}

RobyServo robyServo;