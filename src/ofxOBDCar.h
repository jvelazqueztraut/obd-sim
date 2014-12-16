#pragma once
class ofxOBDCar{
public:
	ofxOBDCar(){
		speed=0;rpm=0;temp=90;nafta=100;runtime=0;
	}

	unsigned int speed;
	unsigned long rpm;
	unsigned char temp;
	unsigned char nafta;
	unsigned char runtime;
};