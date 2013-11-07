//
//  AvatarTouch.h
//  interface
//
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__AvatarTouch__
#define __interface__AvatarTouch__

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <AvatarData.h>

enum AvatarHandState
{
    HAND_STATE_NULL = 0,
    HAND_STATE_OPEN,
    HAND_STATE_GRASPING,
    HAND_STATE_POINTING,
    HAND_STATE_LEAP,
    NUM_HAND_STATES
};

class AvatarTouch {
public:

    AvatarTouch();

    void simulate(float deltaTime);
    void render(glm::vec3 cameraPosition);

    void setHasInteractingOther(bool hasInteractingOther) { _hasInteractingOther = hasInteractingOther;}
    void setMyHandPosition     (glm::vec3   position    ) { _myHandPosition      = position;}
    void setYourHandPosition   (glm::vec3   position    ) { _yourHandPosition    = position;}
    void setMyOrientation      (glm::quat   orientation ) { _myOrientation       = orientation;}
    void setYourOrientation    (glm::quat   orientation ) { _yourOrientation     = orientation;}
    void setMyBodyPosition     (glm::vec3   position    ) { _myBodyPosition      = position;}
    void setYourBodyPosition   (glm::vec3   position    ) { _yourBodyPosition    = position;}
    void setMyHandState        (int         state       ) { _myHandState         = state;}
    void setYourHandState      (int         state       ) { _yourHandState       = state;}
    void setReachableRadius    (float       radius      ) { _reachableRadius     = radius;}
    void setHoldingHands       (bool        holding     ) { _weAreHoldingHands   = holding;}
    
    bool getAbleToReachOtherAvatar () const {return _canReachToOtherAvatar;  }
    bool getHandsCloseEnoughToGrasp() const {return _handsCloseEnoughToGrasp;}
    bool getHoldingHands           () const {return _weAreHoldingHands;      }

private:

    static const int NUM_PARTICLE_POINTS = 100;
    
    bool        _hasInteractingOther;
    bool        _weAreHoldingHands;
    glm::vec3   _point [NUM_PARTICLE_POINTS];
    glm::vec3   _myBodyPosition;
    glm::vec3   _yourBodyPosition;
    glm::vec3   _myHandPosition;
    glm::vec3   _yourHandPosition;
    glm::quat   _myOrientation;
    glm::quat   _yourOrientation;
    glm::vec3   _vectorBetweenHands;
    int         _myHandState;
    int         _yourHandState;
    bool        _canReachToOtherAvatar;
    bool        _handsCloseEnoughToGrasp;
    float       _reachableRadius;
    
    void renderBeamBetweenHands();
};

#endif
