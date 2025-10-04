#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <math.h>

const double toDegrees = 180.0 / M_PI;
const int scale = 3000;

typedef struct Point {
	float x;
	float y;
} Point;

// physics definitions
typedef struct phys_Shaft {
	double x;
	double y;
	double angle;
	double radius;
	double inertia;
	double omega;
	double accel;
	double torque;
	double Mu;
} phys_Shaft;

typedef struct cylinder {
	double bore;
	double stroke;
	double pressure;
	double apressure;
} cylinder;

typedef struct head {
	double intakePressure; 
	double headVolume;
	double exhaustDisplacement;
	double intakeDisplacement;
	double intakeVRadius;
	double intakeSRadius;
	double exhaustVRadius;
	double exhaustSRadius;
} head;

typedef struct piston {
	double rodLength;
        double position; // position relative to tdc
        double velocity;
        double area;
        double inForce;
        double outForce;
	double nForce;
} piston;



void Initialize(){
	SetTargetFPS(2500);
}



// 	double functions

double ForceFromP(cylinder* cyl) {
	double r = cyl->bore / 2;
	double f = cyl->pressure * (M_PI * (r * r));	
	return f;
}

double EffectiveRodLength(double length, double radius, double theta){
	double rcos = radius * cos(theta);
	return sqrt(length * length - (rcos * rcos));
}



double FrustumLSA(double R, double r, double h){
	double LSA = M_PI * (R + r) * sqrt((R-r) * (R-r) + h * h);
	return LSA;	

}

double TcsFromFp(double fp, double theta, double L, double R){
	double n = L/R;
	double Tcs = fp * R * (cos(theta) + (sin(2 * theta))/(2 * sqrt(n * n - sin(theta) * sin(theta))));
	return Tcs;
}



//	void functions

void updPiston(phys_Shaft* crankshaft, cylinder* cylinder, piston* piston, head* head){

	//	this function updates the pistons properties.

	double angle = crankshaft->angle;
	double length = piston->rodLength;
	double cradius = crankshaft->radius;
	double omega = crankshaft->omega;
	double n = length / cradius;
	double sa = sin(angle + M_PI / 2);
	
	piston->area = (cylinder->bore / 2) * (cylinder->bore / 2) * M_PI;	

	piston->position = 
//	EffectiveRodLength(length, cradius, angle + M_PI / 2) + cradius * sa;
	cradius * cos(angle + M_PI / 2) + sqrt(length * length - cradius * cradius * sa * sa);
	
	piston->velocity = 
	-omega * cradius * (sa + (sin(2 * angle + M_PI / 2) / (2 * sqrt(n * n - sa * sa))));

	piston->inForce = cylinder->pressure * piston->area;
	piston->outForce = cylinder->apressure * piston->area;
	piston->nForce = piston->inForce - piston->outForce;
}


void updCylinder(cylinder* cylinder, piston* piston, head* head){
	
	// 	this function updates the cylinders properties.

	double vp = piston->velocity;
	double ap = piston->area;
	double vi = piston->area * cylinder->stroke / 2; //+ head->headVolume;			// temp		// initial volume, volume from when av reached 0
	double vc = piston->area * (cylinder->stroke + piston->rodLength - piston->position); //+ head->headVolume; 	// current volume
	double av = FrustumLSA(head->exhaustVRadius, head->exhaustSRadius, head->exhaustDisplacement);	// the lsa of a frustum through which gas flows
	double pa = cylinder->apressure;
	double pc = 0.0;
	double rho = 1.225; // density of air in kg/m3

	if (av > 0.0001){ // left off here
		double vaa = vp * ap / av;
		pc = 0.5 * rho * (vaa * vaa - vp * vp) + pa;		// use bernoulli principle when valve is open
	} else {
		pc = vi * pa / vc;
		//pc = 0.0;				// when closed, just get the proportional pressure	
	}
	cylinder->pressure = pc;
}


void updShaft(phys_Shaft* shaft,piston* piston, double extraTorque, double dt){

	//	this function updates the crankshafts properties.	

	shaft->torque = TcsFromFp(piston->nForce, shaft->angle, piston->rodLength, shaft->radius) + extraTorque;
	shaft->accel = shaft->torque / shaft->inertia;
	shaft->omega += shaft->accel * dt;
	shaft->omega -= shaft->omega * 10.0 * dt;
	shaft->angle += shaft->omega * dt;
	if (isnan(shaft->omega)){
		shaft->omega = 0;
	}
	if (isnan(shaft->angle)){
		shaft->angle = 0;
	}
}



// misc functions

void drawShaft(phys_Shaft* shaft){
	const Vector2 center = { shaft->x, shaft->y };
	DrawCircle(shaft->x, shaft->y, shaft->radius * scale, RAYWHITE);
	DrawCircleSector(
			center, 
			shaft->radius * scale, 
			shaft->angle * toDegrees + 10, 
			shaft->angle * toDegrees - 10, 
			3, 
			GRAY
		);
}


void drawPoint(Point* point){
	DrawCircle(point->x, point->y, 5, BLACK);
}


int inputx = 0;
int inputy = 0;
void control(){

	if (IsKeyDown(KEY_D)){
		inputx = 1;
	}
	else if (IsKeyDown(KEY_A)){
		inputx = -1;
	} else {
		inputx = 0;
	}
	if (IsKeyDown(KEY_W)){
		inputy = 1;
	}
	else if (IsKeyDown(KEY_S)){
		inputy = -1;
	}
	else {
		inputy = 0;
	}
}



int main(){
	const int width = 1200;
	const int height = 800;
	
	
	phys_Shaft crankshaft = (phys_Shaft){
		.x = 		600,
		.y = 		600,

		.angle = 	0.0,
		.radius = 	0.045,
		.inertia = 	0.09,
		.omega = 	0.0,
		.accel = 	0.0,
		.Mu = 		0.01,
		.torque = 	0.0,
	};
	phys_Shaft *crankshaftPtr = &crankshaft;

	cylinder cyl_1 = (cylinder){
		.bore = 	0.084,		// diameter
		.stroke = 	0.09,		// stroke must be 2 * crankshaft radius
		.pressure = 	3000000.0,
		.apressure =	101325.0,
	};
	cylinder *cyl_1Ptr = &cyl_1;
	
	head head_1 = (head){
		.intakePressure = 	101325.0, 	// 1 atm = 101325 Pa
		.headVolume = 		0.00084, 	// temp, in m^3
		.exhaustDisplacement = 	0.01,  		// valve opening in m
		.intakeDisplacement = 	0.0,
		.intakeVRadius = 	0.0165,		// valve radius
		.intakeSRadius = 	0.015,		// seat radius
		.exhaustVRadius = 	0.0145,
		.exhaustSRadius = 	0.0135,		
	};
	head *head_1Ptr = &head_1;

        piston piston_1 = (piston){
		.rodLength = 	0.14435, // must be longer than stroke
                .position = 	0.0,
                .velocity = 	0.0,
                .area = 	0.0,	// will be updated with cylinder info
                .inForce = 	0.0,
                .outForce = 	0.0,
        };
	piston *piston_1Ptr = &piston_1;	


	Point point_a = (Point){
		.x = 0,
		.y = 0,
	};
	Point *point_aPtr = &point_a;

	Point point_b = (Point){
		.x = 0,
		.y = 0,
	};
	Point *point_bPtr = &point_b;


	InitWindow(width, height, "great");	
	Initialize();
	
    	SetConfigFlags(FLAG_MSAA_4X_HINT);      // Enable Multi Sampling Anti Aliasing 4x (if available)

	while(!WindowShouldClose()) {
		double dt = GetFrameTime();
		control();
		
		head_1Ptr->exhaustDisplacement = 0.01 * inputy;
		updPiston(crankshaftPtr, cyl_1Ptr, piston_1Ptr, head_1Ptr);
		updCylinder(cyl_1Ptr, piston_1Ptr, head_1Ptr);
		updShaft(crankshaftPtr, piston_1Ptr, 100.0 * inputx, dt);
		

		point_aPtr->x = crankshaftPtr->radius * scale * cos(crankshaftPtr->angle) + crankshaftPtr->x;
		point_aPtr->y = crankshaftPtr->radius * scale * sin(crankshaftPtr->angle) + crankshaftPtr->y;
		
		point_bPtr->x = crankshaftPtr->x;
//		point_bPtr->y = crankshaftPtr->y - EffectiveRodLength(piston_1Ptr->rodLength * scale, crankshaftPtr->radius * scale, crankshaftPtr->angle) + crankshaftPtr->radius * scale * sin(crankshaftPtr->angle);
		point_bPtr->y = crankshaftPtr->y -  piston_1Ptr->position * scale;
		


		BeginDrawing();

		// draw stuff
		ClearBackground(SKYBLUE);
		DrawText(TextFormat("dt: %02.09f ms", dt), 10, 10, 10, BLACK);
//		DrawText(TextFormat("theta: %02.09f ms", (crankshaftPtr->omega * 0.159155 * 60)), 10, 30, 20, GRAY);
		DrawText(TextFormat("pressure: %02.09f ms", (cyl_1Ptr->pressure - cyl_1Ptr->apressure)), 10, 30, 20, GRAY);
		DrawText(TextFormat("rpm: %02.09f ", crankshaftPtr->omega * 9.5493), 10, 70, 20, GRAY);
DrawText(TextFormat("dist: %02.09f", sqrt(pow(point_aPtr->x - point_bPtr->x,2) + pow(point_aPtr->y - point_bPtr->y,2))), 10, 50, 20, GRAY);
		// draw crankshaft
		drawShaft(&crankshaft);

		// draw points
		drawPoint(point_aPtr);
		drawPoint(point_bPtr);
		DrawLine(point_aPtr->x, point_aPtr->y, point_bPtr->x, point_bPtr->y, BLACK);
		EndDrawing();
	}
	

	CloseWindow();


	return 0; 
}
