#pragma once
#include "ofMain.h"
#include "ofxOBDCar.h"
#include "obddefines.h"
#include "simport.h"
#include "obdconvertfunctions.h"
#include "obdservicecommands.h"

#define OBDSIM_DEVICE_IDENTIFIER_STRING "ELM327_SIMULATOR"
#define OBDSIM_ELM_DEVICE_STRING "OBDSIM_DEVICE"
#define OBDSIM_ELM_VERSION_STRING "OBDSIM_VERSION"

class ofxOBDSim{
public:
	void open(string port, ofxOBDCar * car);
	void update();
	void close();

	OBDSimPort *sp;
	struct simsettings *ss;
	ofxOBDCar * car;

	char *line; // Single line from the other end of the device

private:
	/// Parse this AT command [assumes that line is already known to be an AT command]
	int parse_ATcmd(struct simsettings *ss, OBDSimPort *sp, char *line, string& response);

	/// Parse this 01 command [assumes that line is already known to be an 01 command]
	int parse_01cmd(struct simsettings *ss, OBDSimPort *sp, char *line, string& response);

	/// Render a header into the passed string
	/** \param buf buffer to put rendered header into
		\param buflen size of buf
		\param proto the obdii protocol we're rendering
		\param ecu the ecu this message is from
		\param messagelen the number of bytes being returned as the message itself
		\param spaces whether or not to put spaces between the characters [and at the end]
		\param dlc whether or not to put dlc byte in
		\return length of string put in buf
	*/
	int render_obdheader(char *buf, size_t buflen, struct obdiiprotocol *proto,
		struct obdgen_ecu *ecu, unsigned int messagelen, int spaces, int dlc);

	/// Update the freeze frame info for all the ecus
	void obdsim_freezeframes(struct obdgen_ecu *ecus, int ecucount);
};