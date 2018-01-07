union UpsStatus{
	uint8_t buf[8];
	struct option{
		bool UtilityFail;
		bool BatteryLow;
		bool BypassBoost;
		bool UPSFailed;
		bool UPSIsOnline;
		bool TestInProgress;
		bool ShutdownActive;
		bool BeeperOn;
	} option;
};

class UPSInfo {
private:
	float IpVoltage;
	float IpFaultVoltage;
	float OpVoltage;
	int OpCurrent;
	float IpFrequency;
	float BatVoltage;
	float Temperature;
	UpsStatus Status;
	bool isConnected = false;
	unsigned long tsUpdate = 0;
	unsigned long tsUpdateConnection = 0;

	float getFloatVal(char* source, int num){
		char val[10] = { 0 };
		strncpy(val, source, num);
		return atof(val);
	}
	
	int getIntVal(char* source, int num){
		char val[10] = { 0 };
		strncpy(val, source, num);
		return atoi(val);
	}
public:
	float getIpVoltage(){
		return IpVoltage;
	}

	float getIpFaultVoltage(){
		return IpFaultVoltage;
	}

	float getOpVoltage(){
		return OpVoltage;
	}

	int getOpCurrent(){
		return OpCurrent;
	}
	
	float getIpFrequency(){
		return IpFrequency;
	}

	float getBatVoltage(){
		return BatVoltage;
	}

	float getTemperature(){
		return Temperature;
	}

	UpsStatus getStatus(){
		return Status;
	}

    bool isConnectedUps(){
		return this->isConnected;
	}

	void update(){
		if (tsUpdate < millis()){		
			char buf[50] = {0};
			Serial.print("Q1\r");			
			Serial.flush();
			int rcvBytes = Serial.readBytes(buf, 47);	
			if (rcvBytes == 47){
				this->IpVoltage = getFloatVal(&buf[1], 5);
				this->IpFaultVoltage = getFloatVal(&buf[7], 5);
				this->OpVoltage = getFloatVal(&buf[13], 5);
				this->OpCurrent = getIntVal(&buf[19], 3);
				this->IpFrequency = getFloatVal(&buf[23], 4);
				this->BatVoltage = getFloatVal(&buf[28], 4);
				this->Temperature = getFloatVal(&buf[33], 4);
				char *ptr = &buf[38];
				for (int i = 0; i < 9; i++){
					this->Status.buf[i] = (ptr[0] == '1');
					ptr++;
				}
				isConnected = true;
			}
			tsUpdate = millis() + TIMEOUT_CHECK_UPS;
		}
	}
};