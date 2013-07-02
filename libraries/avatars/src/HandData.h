//
//  HandData.h
//  hifi
//
//  Created by Eric Johnston on 6/26/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#ifndef __hifi__HandData__
#define __hifi__HandData__

#include <iostream>
#include <vector>

#include <glm/glm.hpp>

class AvatarData;

class HandData {
public:
    HandData(AvatarData* owningAvatar);
    
    const std::vector<glm::vec3>& getFingerTips() const { return _fingerTips; }
    const std::vector<glm::vec3>& getFingerRoots() const { return _fingerRoots; }
    void setFingerTips(const std::vector<glm::vec3>& fingerTips) { _fingerTips = fingerTips; }
    void setFingerRoots(const std::vector<glm::vec3>& fingerRoots) { _fingerRoots = fingerRoots; }
    
    friend class AvatarData;
protected:
    std::vector<glm::vec3> _fingerTips;
    std::vector<glm::vec3> _fingerRoots;
    AvatarData* _owningAvatarData;
private:
    // privatize copy ctor and assignment operator so copies of this object cannot be made
    HandData(const HandData&);
    HandData& operator= (const HandData&);
};

#endif /* defined(__hifi__HandData__) */
