#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
	ofSetWindowTitle("OBDSIM");
	ofSetFrameRate(30);
	ofSetLogLevel(OF_LOG_VERBOSE);

	string winport="COM27";
	simulator.open(winport,&car);

	//CHECK IF THERE EVEN IS A GAMEPAD CONNECTED
	if(ofxGamepadHandler::get()->getNumPads()>0){
			ofxGamepad* pad = ofxGamepadHandler::get()->getGamepad(0);
			ofAddListener(pad->onAxisChanged, this, &testApp::axisChanged);
			ofAddListener(pad->onButtonPressed, this, &testApp::buttonPressed);
			ofAddListener(pad->onButtonReleased, this, &testApp::buttonReleased);
	}

	frame=0;
}

//--------------------------------------------------------------
void testApp::update(){
	
	simulator.update();

	frame++;
}

//--------------------------------------------------------------
void testApp::draw(){
	ofBackground(0);
	ofSetColor(255);
	ofDrawBitmapString(ofToString(frame),10,20);
}

//--------------------------------------------------------------
void testApp::exit(){
	simulator.close();
}

//--------------------------------------------------------------
void testApp::axisChanged(ofxGamepadAxisEvent& e)
{
	cout << "AXIS " << e.axis << " VALUE " << ofToString(e.value) << endl;
}

//--------------------------------------------------------------
void testApp::buttonPressed(ofxGamepadButtonEvent& e)
{
	cout << "BUTTON " << e.button << " PRESSED" << endl;
}

//--------------------------------------------------------------
void testApp::buttonReleased(ofxGamepadButtonEvent& e)
{
	cout << "BUTTON " << e.button << " RELEASED" << endl;
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){

}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}
