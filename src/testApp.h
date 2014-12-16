#pragma once

#include "ofMain.h"
#include "obddefines.h"
#include "simport.h"
#include "obdconvertfunctions.h"
#include "obdservicecommands.h"

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

class testApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();
		void exit();

		void keyPressed  (int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		OBDSimPort *sp;

		struct simsettings *ss;

		unsigned int frame;
};
