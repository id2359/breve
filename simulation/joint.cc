#include "simulation.h"
#include "joint.h"

/*!
	\brief Applies a torque to a revolute joint.
*/

void slJoint::applyTorque(slVector *torque) {
	slVector t;
	dVector3 axis;

	dJointGetHingeAxis(_odeJointID, axis);

	t.x = axis[0] * torque->x;
	t.y = axis[1] * torque->x;
	t.z = axis[2] * torque->x;

	if(_parent) dBodySetTorque(_parent->_odeBodyID, t.x, t.y, t.z);
   	if(_child) dBodySetTorque(_child->_odeBodyID, -t.x, -t.y, -t.z);
}

void slJoint::setNormal(slVector *normal) {
	slVector tn;

	// transform the normal to the parent's frame

	if(_parent) slVectorXform(_parent->position.rotation, normal, &tn);
	else slVectorCopy(normal, &tn);

	if(_type == JT_REVOLUTE) {
		dJointSetHingeAxis(_odeJointID, tn.x, tn.y, tn.z);
	} else if(_type == JT_PRISMATIC) {
		dJointSetSliderAxis(_odeJointID, tn.x, tn.y, tn.z);
	}	
}

slJoint::~slJoint() {
	breakJoint();
}

/*!
	\brief Breaks an slJoint struct.

	This triggers an automatic recomputation of multibodies.
*/

void slJoint::breakJoint() {
	slLink *parent = _parent, *child = _child;
	slMultibody *parentBody = NULL, *childBody, *newMb;
	std::vector<slJoint*>::iterator ji;

	if(!parent && !child) return;

	childBody = _child->multibody;

	if(parent) parentBody = parent->multibody;

	if(parent) {
		ji = std::find(parent->outJoints.begin(), parent->outJoints.end(), this);
		if(ji != parent->outJoints.end()) parent->outJoints.erase(ji);
	}

	ji = std::find(child->inJoints.begin(), child->inJoints.end(), this);
	if(ji != child->inJoints.end()) child->inJoints.erase(ji);

	dJointAttach(_odeJointID, NULL, NULL);
	dJointDestroy(_odeJointID);

	if(_odeMotorID) {
		dJointAttach(_odeMotorID, NULL, NULL);
		dJointDestroy(_odeMotorID);
	}

	_child = NULL;
	_parent = NULL;

	if(parentBody) parentBody->update();
	if(childBody && childBody != parentBody) childBody->update();

	// figure out if the broken links are still part of those bodies
	// ... if not, then try to adopt the links 
	// ... if not, then NULL the multibody entries 

	if(parentBody && (std::find(parentBody->_links.begin(), parentBody->_links.end(), parent) == parentBody->_links.end())) {
		if((newMb = slLinkFindMultibody(parent))) newMb->update();
		else parent->nullMultibodiesForConnectedLinks();
	}

	if(childBody && (std::find(childBody->_links.begin(), childBody->_links.end(), child) == childBody->_links.end())) {
		if((newMb = slLinkFindMultibody(child))) newMb->update();
		else child->nullMultibodiesForConnectedLinks();
	}
}

void slJoint::getPosition(slVector *r) {
	switch(_type) {
		case JT_REVOLUTE:
			r->x = dJointGetHingeAngle(_odeJointID);
			r->y = 0;
			r->z = 0;
			break;
		case JT_PRISMATIC:
			r->x = dJointGetSliderPosition(_odeJointID);
			r->y = 0;
			r->z = 0;
			break;
		case JT_BALL:
			r->x = dJointGetAMotorAngle(_odeMotorID, 0);
			r->y = dJointGetAMotorAngle(_odeMotorID, 1);
			r->z = dJointGetAMotorAngle(_odeMotorID, 2);
			break;
		case JT_UNIVERSAL:
			r->x = dJointGetAMotorAngle(_odeMotorID, 0);
			r->y = dJointGetAMotorAngle(_odeMotorID, 2);
			r->z = 0;
			break;
		default:
			break;
	}

	return;
}

void slJoint::getVelocity(slVector *velocity) {
	switch(_type) {
		case JT_REVOLUTE:
			velocity->x = dJointGetHingeAngleRate(_odeJointID);
			break;
		case JT_PRISMATIC:
			velocity->x = dJointGetSliderPositionRate(_odeJointID);
			break;
		case JT_BALL:
			velocity->z = dJointGetAMotorParam(_odeMotorID, dParamVel3);
		case JT_UNIVERSAL:
			velocity->x = dJointGetAMotorParam(_odeMotorID, dParamVel);
			velocity->y = dJointGetAMotorParam(_odeMotorID, dParamVel2);
			break;
	}
}

void slJoint::setVelocity(slVector *speed) {
	_targetSpeed = speed->x;

	// if(_type == JT_REVOLUTE) dJointSetHingeParam (_odeJointID, dParamVel, speed->x);
	// else if(_type == JT_PRISMATIC) dJointSetSliderParam (_odeJointID, dParamVel, speed->x);

	if(_type == JT_UNIVERSAL) {
		dJointSetAMotorParam(_odeMotorID, dParamVel, speed->x);
		dJointSetAMotorParam(_odeMotorID, dParamVel3, speed->y);
	} else if(_type == JT_BALL) {
		dJointSetAMotorParam(_odeMotorID, dParamVel, speed->x);
		dJointSetAMotorParam(_odeMotorID, dParamVel2, speed->y);
		dJointSetAMotorParam(_odeMotorID, dParamVel3, speed->z);
	}
}

/*!
	\brief Sets minima and maxima for a joint.

	The minima and maxima are relative to the joint's natural state.
	Since the joint may be 1-, 2- or 3-DOF, only the relevant field
	of the vectors are used.
*/

void slJoint::setLimits(slVector *min, slVector *max) {
	switch(_type) {
		case JT_PRISMATIC:	
			dJointSetSliderParam(_odeJointID, dParamStopERP, .1);
			dJointSetSliderParam(_odeJointID, dParamLoStop, min->x);
			dJointSetSliderParam(_odeJointID, dParamHiStop, max->x);
			break;
		case JT_REVOLUTE:	
			dJointSetHingeParam(_odeJointID, dParamStopERP, .1);
			dJointSetHingeParam(_odeJointID, dParamLoStop, min->x);
			dJointSetHingeParam(_odeJointID, dParamHiStop, max->x);
			break;
		case JT_BALL:	
			// The ODE user manual states that there is a singularity when
			// the a1 angle goes outside of the range (-pi / 2, pi / 2).  I
			// see clearly incorrect behaviors already at -pi / 2 + 0.2 and
			// oddities even earlier, so we'll stay far away from that area.

			if(max->y >= (M_PI/2.0) - 0.35) max->y = (M_PI/2.0) - 0.35;
			if(min->y <= (-M_PI/2.0) + 0.35) min->y = -(M_PI/2.0) + 0.35;

			dJointSetAMotorParam(_odeMotorID, dParamLoStop, min->x);
			dJointSetAMotorParam(_odeMotorID, dParamLoStop2, min->y);
			dJointSetAMotorParam(_odeMotorID, dParamLoStop3, min->z);
			dJointSetAMotorParam(_odeMotorID, dParamStopERP, .1);
			dJointSetAMotorParam(_odeMotorID, dParamStopERP2, .1);
			dJointSetAMotorParam(_odeMotorID, dParamStopERP3, .1);
			dJointSetAMotorParam(_odeMotorID, dParamHiStop, max->x);
			dJointSetAMotorParam(_odeMotorID, dParamHiStop2, max->y);
			dJointSetAMotorParam(_odeMotorID, dParamHiStop3, max->z);
			break;
		case JT_UNIVERSAL:
			dJointSetAMotorParam(_odeMotorID, dParamLoStop, min->x);
			dJointSetAMotorParam(_odeMotorID, dParamLoStop3, min->y);
			dJointSetAMotorParam(_odeMotorID, dParamStopERP, .1);
			dJointSetAMotorParam(_odeMotorID, dParamStopERP3, .1);
			dJointSetAMotorParam(_odeMotorID, dParamHiStop, max->x);
			dJointSetAMotorParam(_odeMotorID, dParamHiStop3, max->y);
			break;
	}
}

/*!
	\brief Set the maximum torque that a joint can affect.
*/

void slJoint::setMaxTorque(double max) {
	switch(_type) {
		case JT_REVOLUTE:
			dJointSetHingeParam(_odeJointID, dParamFMax, max);
			break;
		case JT_PRISMATIC:
			dJointSetSliderParam(_odeJointID, dParamFMax, max);
			break;
		case JT_UNIVERSAL:
			dJointSetAMotorParam(_odeMotorID, dParamFMax, max);
			dJointSetAMotorParam(_odeMotorID, dParamFMax3, max);
			break;
		case JT_BALL:
			dJointSetAMotorParam(_odeMotorID, dParamFMax, max);
			dJointSetAMotorParam(_odeMotorID, dParamFMax2, max);
			dJointSetAMotorParam(_odeMotorID, dParamFMax3, max);
			break;
	}
}

/*!
	\brief Modifies the link points of a joint.
*/

void slJoint::setLinkPoints(slVector *plinkPoint, slVector *clinkPoint, double rotation[3][3], int first = 0) {
	const double *childR;
	dReal *childP;
	dReal idealR[16];
	dReal savedChildR[16], savedChildP[3];
	slVector hingePosition, childPosition;
	double ideal[3][3];

	childP = dBodyGetPosition(_child->_odeBodyID);
	childR = dBodyGetRotation(_child->_odeBodyID);
	memcpy(savedChildR, childR, sizeof(savedChildR));
	memcpy(savedChildP, childP, sizeof(savedChildP));

	if (_parent)
	   slMatrixMulMatrix(_parent->position.rotation, rotation, ideal);
	else
	   slMatrixCopy(rotation, ideal);		

	slSlToODEMatrix(ideal, idealR);

	// compute the hinge position--the plinkPoint in world coordinates 

	if(_parent) {
		slVectorXform(_parent->position.rotation, plinkPoint, &hingePosition);
		slVectorAdd(&hingePosition, &_parent->position.location, &hingePosition);
	} else {
		slVectorCopy(plinkPoint, &hingePosition);
	}

	// set the ideal positions, so that the anchor 
	// command registers the native position 

	slVectorXform(ideal, clinkPoint, &childPosition);
	slVectorSub(&hingePosition, &childPosition, &childPosition);

	dJointAttach(_odeJointID, NULL, NULL);

	dBodySetRotation(_child->_odeBodyID, idealR);
	dBodySetPosition(_child->_odeBodyID, childPosition.x, childPosition.y, childPosition.z);

	//
	// NOTE: the child is first in the attachment because of a bug in an older version
	// of ODE.
	//

	if(_parent) dJointAttach(_odeJointID, _child->_odeBodyID, _parent->_odeBodyID);
	else dJointAttach(_odeJointID, _child->_odeBodyID, NULL);

	switch(_type) {
		case JT_REVOLUTE:
			dJointSetHingeAnchor(_odeJointID, hingePosition.x, hingePosition.y, hingePosition.z);
			break;
		case JT_FIX:
			dJointSetFixed(_odeJointID);
			break;
		case JT_UNIVERSAL:
			dJointSetUniversalAnchor(_odeJointID, hingePosition.x, hingePosition.y, hingePosition.z);
			break;
		case JT_BALL:
			dJointSetBallAnchor(_odeJointID, hingePosition.x, hingePosition.y, hingePosition.z);
			break;
		default:
			break;
	}

	// set the proper positions where the link should actually be at this time 

	slVectorXform(_child->position.rotation, clinkPoint, &childPosition);
	slVectorSub(&hingePosition, &childPosition, &childPosition);

	if( !first) {
		dBodySetRotation(_child->_odeBodyID, savedChildR);
	}

	dBodySetPosition(_child->_odeBodyID, childPosition.x, childPosition.y, childPosition.z);

	if(_parent) _parent->updatePositions();
	_child->updatePositions();
}
