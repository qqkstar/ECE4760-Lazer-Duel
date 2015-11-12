// PID 

float RPM;  // The RPM measured by the other code
//int captureTime;  // The time RPM was captured
//int lastCaptureTime;  // The time the previous RPM was captured
float desiredRPM;  // The RPM we want to set the motor to

float error; // The difference between the desired RPM and the RPM measured 
float prevError;  // The error of the previous measurement
float derivativeError;  // Derivative of the error
float integralError;  // Integral of the error

float P;  // Proportional gain term
float I;  // Integral gain term
float D;  // Derivative gain term

float outputPID;  // The control signal calculated by the PID algorithm

float PID(float RPM, float prevRPM, float P, float I, float D){
	// Calculate the error
	error = desiredRPM - RPM;
	// Calculate integral term
	integralError = integralError + error;
	// Calculate derivative term
	derivativeError = error - prevError;
	
	// Calculate the control signal
	outputPID = P*error + I*integralError + D*derivativeError;
}