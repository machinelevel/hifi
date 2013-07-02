//
//  Hand.cpp
//  interface
//
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.

#include <QImage>

#include <AgentList.h>

#include "Application.h"
#include "Avatar.h"
#include "Hand.h"
#include "Util.h"
#include "renderer/ProgramObject.h"

using namespace std;

Hand::Hand(Avatar* owningAvatar) :
    HandData((AvatarData*)owningAvatar),
    _owningAvatar(owningAvatar),
    _renderAlpha(1.0),
    _lookingInMirror(false),
    _ballColor(0.0, 0.0, 0.4),
    _position(0.0, 0.4, 0.0),
    _orientation(0.0, 0.0, 0.0, 1.0)
{
}

void Hand::init() {
    // Different colors for my hand and others' hands
    if (_owningAvatar && _owningAvatar->isMyAvatar())
        _ballColor = glm::vec3(0.0, 0.4, 0.0);
    else
        _ballColor = glm::vec3(0.0, 0.0, 0.4);
}

void Hand::reset() {
}

void Hand::simulate(float deltaTime, bool isMine) {
}

glm::vec3 Hand::leapPosToWorldPos(const glm::vec3& leapPos) {
    float unitScale = 0.001; // convert mm to meters
    return _position + _orientation * (leapPos * unitScale);
}

void Hand::calculateGeometry() {
    glm::vec3 offset(0.2, -0.2, -0.3);  // place the hand in front of the face where we can see it
    
    Head& head = _owningAvatar->getHead();
    _position = head.getPosition() + head.getOrientation() * offset;
    _orientation = head.getOrientation();

    int numLeapBalls = _fingerTips.size() + _fingerRoots.size();
    _leapBalls.resize(numLeapBalls);
    
    int ball = 0;
    for (int i = 0; i < _fingerTips.size(); ++i) {
        _leapBalls[ball].rotation = _orientation;
        _leapBalls[ball].position = leapPosToWorldPos(_fingerTips[i]);
        _leapBalls[ball].radius         = 0.01;
        _leapBalls[ball].touchForce     = 0.0;
        _leapBalls[ball].isCollidable   = true;
        ball++;
    }
    
    bool rootBalls = false; // enable this to draw ballls at the finger roots
    if (rootBalls) {
        for (int i = 0; i < _fingerRoots.size(); ++i) {
            _leapBalls[ball].rotation = _orientation;
            _leapBalls[ball].position = leapPosToWorldPos(_fingerRoots[i]);
            _leapBalls[ball].radius         = 0.005;
            _leapBalls[ball].touchForce     = 0.0;
            _leapBalls[ball].isCollidable   = true;
            ball++;
        }
    }
}


void Hand::render(bool lookingInMirror) {

    _renderAlpha = 1.0;
    _lookingInMirror = lookingInMirror;
    
    calculateGeometry();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_RESCALE_NORMAL);
    
    renderHandSpheres();
}

void Hand::renderHandSpheres() {
    glPushMatrix();
    // Draw the leap balls
    for (size_t i = 0; i < _leapBalls.size(); i++) {
        float alpha = 1.0f;
        
        if (alpha > 0.0f) {
            glColor4f(_ballColor.r, _ballColor.g, _ballColor.b, alpha);
            
            glPushMatrix();
            glTranslatef(_leapBalls[i].position.x, _leapBalls[i].position.y, _leapBalls[i].position.z);
            glutSolidSphere(_leapBalls[i].radius, 20.0f, 20.0f);
            glPopMatrix();
        }
    }
    
    if (_fingerTips.size() == _fingerRoots.size()) {
        for (size_t i = 0; i < _fingerTips.size(); ++i) {
            glColor4f(_ballColor.r, _ballColor.g, _ballColor.b, 0.5);
            glm::vec3 tip = leapPosToWorldPos(_fingerTips[i]);
            glm::vec3 root = leapPosToWorldPos(_fingerRoots[i]);
            Avatar::renderJointConnectingCone(root, tip, 0.0025, 0.005);
        }
    }
    glPopMatrix();
}

void Hand::setLeapFingers(const std::vector<glm::vec3>& fingerTips,
                          const std::vector<glm::vec3>& fingerRoots) {
    _fingerTips = fingerTips;
    _fingerRoots = fingerRoots;
}


