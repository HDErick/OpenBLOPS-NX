// Named scalar fields adapted from campaign-save-wip. No native layout offsets.
    v.Field("species", actor.species);
    v.Field("state.actorNum", actor.state.actorNum);
    v.Field("state.entityNum", actor.state.entityNum);
    v.Field("state.team", actor.state.team);
    for (unsigned int i0 = 0; i0 != 6; ++i0)
    {
        v.Field("state.attachTagIndex[i0]", actor.state.attachTagIndex[i0]);
    }
    v.Field("state.attachIgnoreCollision", actor.state.attachIgnoreCollision);
    for (unsigned int i0 = 0; i0 != 32; ++i0)
    {
        v.Field("state.name[i0]", actor.state.name[i0]);
    }
    v.Field("state.attachedEntNum", actor.state.attachedEntNum);
    v.Field("state.attachedEntTime", actor.state.attachedEntTime);
    v.Field("state.attachedTagIndex", actor.state.attachedTagIndex);
    v.Field("state.animScriptedAnim", actor.state.animScriptedAnim);
    v.Field("state.animScriptedAnimTime", actor.state.animScriptedAnimTime);
    v.Field("state.hudwarningType", actor.state.hudwarningType);
    v.Field("state.lookAtEntNum", actor.state.lookAtEntNum);
    v.Field("legsAnim", actor.legsAnim);
    v.Field("legsTimer", actor.legsTimer);
    v.Field("legsAnimDuration", actor.legsAnimDuration);
    for (unsigned int i0 = 0; i0 != 6; ++i0)
    {
        v.Field("eState[i0]", actor.eState[i0]);
    }
    for (unsigned int i0 = 0; i0 != 6; ++i0)
    {
        v.Field("eSubState[i0]", actor.eSubState[i0]);
    }
    v.Field("stateLevel", actor.stateLevel);
    v.Field("iStateTime", actor.iStateTime);
    v.Field("preThinkTime", actor.preThinkTime);
    for (unsigned int i0 = 0; i0 != 13; ++i0)
    {
        v.Field("StateTransitions[i0].eTransition", actor.StateTransitions[i0].eTransition);
        v.Field("StateTransitions[i0].eState", actor.StateTransitions[i0].eState);
    }
    v.Field("transitionCount", actor.transitionCount);
    for (unsigned int i0 = 0; i0 != 6; ++i0)
    {
        v.Field("eSimulatedState[i0]", actor.eSimulatedState[i0]);
    }
    v.Field("simulatedStateLevel", actor.simulatedStateLevel);
    v.Field("deathContents", actor.deathContents);
    v.Field("iPainTime", actor.iPainTime);
    v.Field("iReactTime", actor.iReactTime);
    v.Field("allowReact", actor.allowReact);
    v.Field("lastBulletHitTime", actor.lastBulletHitTime);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("reactOrigin[i0]", actor.reactOrigin[i0]);
    }
    v.Field("allowPain", actor.allowPain);
    v.Field("allowDeath", actor.allowDeath);
    v.Field("delayedDeath", actor.delayedDeath);
    v.Field("provideCoveringFire", actor.provideCoveringFire);
    v.Field("lastShotTime", actor.lastShotTime);
    v.Field("fLookPitch", actor.fLookPitch);
    v.Field("fLookYaw", actor.fLookYaw);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("vLookForward[i0]", actor.vLookForward[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("vLookRight[i0]", actor.vLookRight[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("vLookUp[i0]", actor.vLookUp[i0]);
    }
    v.Field("CodeOrient.eMode", actor.CodeOrient.eMode);
    v.Field("CodeOrient.fDesiredLookPitch", actor.CodeOrient.fDesiredLookPitch);
    v.Field("CodeOrient.fDesiredLookYaw", actor.CodeOrient.fDesiredLookYaw);
    v.Field("CodeOrient.fDesiredBodyYaw", actor.CodeOrient.fDesiredBodyYaw);
    v.Field("ScriptOrient.eMode", actor.ScriptOrient.eMode);
    v.Field("ScriptOrient.fDesiredLookPitch", actor.ScriptOrient.fDesiredLookPitch);
    v.Field("ScriptOrient.fDesiredLookYaw", actor.ScriptOrient.fDesiredLookYaw);
    v.Field("ScriptOrient.fDesiredBodyYaw", actor.ScriptOrient.fDesiredBodyYaw);
    v.Field("fDesiredBodyYaw", actor.fDesiredBodyYaw);
    v.Field("fInvProneAnimLowPitch", actor.fInvProneAnimLowPitch);
    v.Field("fInvProneAnimHighPitch", actor.fInvProneAnimHighPitch);
    v.Field("fProneLastDiff", actor.fProneLastDiff);
    v.Field("bProneOK", actor.bProneOK);
    v.Field("ProneInfo.bCorpseOrientation", actor.ProneInfo.bCorpseOrientation);
    v.Field("ProneInfo.orientPitch", actor.ProneInfo.orientPitch);
    v.Field("ProneInfo.prone", actor.ProneInfo.prone);
    v.Field("ProneInfo.iProneTime", actor.ProneInfo.iProneTime);
    v.Field("ProneInfo.iProneTrans", actor.ProneInfo.iProneTrans);
    v.Field("ProneInfo.fBodyHeight", actor.ProneInfo.fBodyHeight);
    v.Field("ProneInfo.fTorsoPitch", actor.ProneInfo.fTorsoPitch);
    v.Field("ProneInfo.fBodyPitch", actor.ProneInfo.fBodyPitch);
    v.Field("ProneInfo.fWaistPitch", actor.ProneInfo.fWaistPitch);
    v.Field("ProneInfo.fBodyRoll", actor.ProneInfo.fBodyRoll);
    v.Field("eyeInfo.time", actor.eyeInfo.time);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("eyeInfo.pos[i0]", actor.eyeInfo.pos[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("eyeInfo.dir[i0]", actor.eyeInfo.dir[i0]);
    }
    v.Field("muzzleInfo.time", actor.muzzleInfo.time);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("muzzleInfo.pos[i0]", actor.muzzleInfo.pos[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("muzzleInfo.dir[i0]", actor.muzzleInfo.dir[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("lookAtInfo.vLookAtPos[i0]", actor.lookAtInfo.vLookAtPos[i0]);
    }
    v.Field("lookAtInfo.fLookAtTurnAngle", actor.lookAtInfo.fLookAtTurnAngle);
    v.Field("lookAtInfo.fLookAtTurnSpeed", actor.lookAtInfo.fLookAtTurnSpeed);
    v.Field("lookAtInfo.fLookAtTurnAccel", actor.lookAtInfo.fLookAtTurnAccel);
    v.Field("lookAtInfo.fLookAtAnimYawLimit", actor.lookAtInfo.fLookAtAnimYawLimit);
    v.Field("lookAtInfo.fLookAtYawLimit", actor.lookAtInfo.fLookAtYawLimit);
    v.Field("lookAtInfo.animLookAtStraight", actor.lookAtInfo.animLookAtStraight);
    v.Field("lookAtInfo.animLookAtLeft", actor.lookAtInfo.animLookAtLeft);
    v.Field("lookAtInfo.animLookAtRight", actor.lookAtInfo.animLookAtRight);
    v.Field("lookAtInfo.bDoLookAt", actor.lookAtInfo.bDoLookAt);
    v.Field("lookAtInfo.bLookAtSetup", actor.lookAtInfo.bLookAtSetup);
    v.Field("lookAtInfo.iLookAtBlendEndTime", actor.lookAtInfo.iLookAtBlendEndTime);
    v.Field("lookAtInfo.fLookAtAnimBlendRate", actor.lookAtInfo.fLookAtAnimBlendRate);
    v.Field("lookAtInfo.fLookAtLimitBlendRate", actor.lookAtInfo.fLookAtLimitBlendRate);
    v.Field("accuracy", actor.accuracy);
    v.Field("playerSightAccuracy", actor.playerSightAccuracy);
    v.Field("missCount", actor.missCount);
    v.Field("hitCount", actor.hitCount);
    v.Field("debugLastAccuracy", actor.debugLastAccuracy);
    v.Field("debugWeaponAccuracy", actor.debugWeaponAccuracy);
    v.Field("iTraceCount", actor.iTraceCount);
    v.Field("iDamageTaken", actor.iDamageTaken);
    v.Field("iDamageYaw", actor.iDamageYaw);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("damageDir[i0]", actor.damageDir[i0]);
    }
    v.Field("eAllowedStances", actor.eAllowedStances);
    v.Field("eTraverseMode", actor.eTraverseMode);
    v.Field("moveMode", actor.moveMode);
    v.Field("safeToChangeScript", actor.safeToChangeScript);
    v.Field("bUseGoalWeight", actor.bUseGoalWeight);
    v.Field("eAnimMode", actor.eAnimMode);
    v.Field("eScriptSetAnimMode", actor.eScriptSetAnimMode);
    v.Field("fAnimTranslationScale", actor.fAnimTranslationScale);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Physics.vOrigin[i0]", actor.Physics.vOrigin[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Physics.vVelocity[i0]", actor.Physics.vVelocity[i0]);
    }
    v.Field("Physics.groundEntNum", actor.Physics.groundEntNum);
    v.Field("Physics.iFootstepTimer", actor.Physics.iFootstepTimer);
    v.Field("Physics.bHasGroundPlane", actor.Physics.bHasGroundPlane);
    v.Field("Physics.groundplaneSlope", actor.Physics.groundplaneSlope);
    v.Field("Physics.iSurfaceType", actor.Physics.iSurfaceType);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Physics.vWishDelta[i0]", actor.Physics.vWishDelta[i0]);
    }
    v.Field("Physics.bIsAlive", actor.Physics.bIsAlive);
    v.Field("Physics.iEntNum", actor.Physics.iEntNum);
    v.Field("Physics.ePhysicsType", actor.Physics.ePhysicsType);
    v.Field("Physics.fGravity", actor.Physics.fGravity);
    v.Field("Physics.iMsec", actor.Physics.iMsec);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Physics.vMins[i0]", actor.Physics.vMins[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Physics.vMaxs[i0]", actor.Physics.vMaxs[i0]);
    }
    v.Field("Physics.prone", actor.Physics.prone);
    v.Field("Physics.iTraceMask", actor.Physics.iTraceMask);
    v.Field("Physics.foliageSoundTime", actor.Physics.foliageSoundTime);
    v.Field("Physics.iNumTouch", actor.Physics.iNumTouch);
    for (unsigned int i0 = 0; i0 != 32; ++i0)
    {
        v.Field("Physics.iTouchEnts[i0]", actor.Physics.iTouchEnts[i0]);
    }
    v.Field("Physics.iHitEntnum", actor.Physics.iHitEntnum);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Physics.vHitOrigin[i0]", actor.Physics.vHitOrigin[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Physics.vHitNormal[i0]", actor.Physics.vHitNormal[i0]);
    }
    v.Field("Physics.bStuck", actor.Physics.bStuck);
    v.Field("Physics.bDeflected", actor.Physics.bDeflected);
    for (unsigned int i0 = 0; i0 != 32; ++i0)
    {
        for (unsigned int i1 = 0; i1 != 3; ++i1)
        {
            v.Field("Path.pts[i0].vOrigPoint[i1]", actor.Path.pts[i0].vOrigPoint[i1]);
        }
        for (unsigned int i1 = 0; i1 != 2; ++i1)
        {
            v.Field("Path.pts[i0].fDir2D[i1]", actor.Path.pts[i0].fDir2D[i1]);
        }
        v.Field("Path.pts[i0].fOrigLength", actor.Path.pts[i0].fOrigLength);
        v.Field("Path.pts[i0].iNodeNum", actor.Path.pts[i0].iNodeNum);
    }
    v.Field("Path.wPathLen", actor.Path.wPathLen);
    v.Field("Path.wOrigPathLen", actor.Path.wOrigPathLen);
    v.Field("Path.wDodgeCount", actor.Path.wDodgeCount);
    v.Field("Path.wNegotiationStartNode", actor.Path.wNegotiationStartNode);
    v.Field("Path.lookaheadNextNode", actor.Path.lookaheadNextNode);
    v.Field("Path.wDodgeEntity", actor.Path.wDodgeEntity);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Path.vFinalGoal[i0]", actor.Path.vFinalGoal[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Path.lookaheadDir[i0]", actor.Path.lookaheadDir[i0]);
    }
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("Path.forwardLookaheadDir2D[i0]", actor.Path.forwardLookaheadDir2D[i0]);
    }
    v.Field("Path.fLookaheadDist", actor.Path.fLookaheadDist);
    v.Field("Path.fLookaheadAmount", actor.Path.fLookaheadAmount);
    v.Field("Path.fLookaheadDistToNextNode", actor.Path.fLookaheadDistToNextNode);
    v.Field("Path.minLookAheadNodes", actor.Path.minLookAheadNodes);
    v.Field("Path.flags", actor.Path.flags);
    v.Field("Path.iPathTime", actor.Path.iPathTime);
    v.Field("Path.eTeam", actor.Path.eTeam);
    v.Field("Path.fCurrLength", actor.Path.fCurrLength);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("Path.vCurrPoint[i0]", actor.Path.vCurrPoint[i0]);
    }
    v.Field("Path.iPathEndTime", actor.Path.iPathEndTime);
    v.Field("Path.pathEndAnimDistSq", actor.Path.pathEndAnimDistSq);
    v.Field("Path.pathEndAnimNotified", actor.Path.pathEndAnimNotified);
    v.Field("Path.numIncreases", actor.Path.numIncreases);
    v.Field("Path.numReductions", actor.Path.numReductions);
    v.Field("fWalkDist", actor.fWalkDist);
    v.Field("TrimInfo.iIndex", actor.TrimInfo.iIndex);
    v.Field("TrimInfo.iDelta", actor.TrimInfo.iDelta);
    v.Field("fInterval", actor.fInterval);
    v.Field("pathWaitTime", actor.pathWaitTime);
    v.Field("iTeamMoveWaitTime", actor.iTeamMoveWaitTime);
    v.Field("iTeamMoveDodgeTime", actor.iTeamMoveDodgeTime);
    v.Field("bDontAvoidPlayer", actor.bDontAvoidPlayer);
    v.Field("sideMove", actor.sideMove);
    v.Field("keepClaimedNode", actor.keepClaimedNode);
    v.Field("keepClaimedNodeInGoal", actor.keepClaimedNodeInGoal);
    v.Field("keepNodeDuringScriptedAnim", actor.keepNodeDuringScriptedAnim);
    v.Field("noDodgeMove", actor.noDodgeMove);
    v.Field("mayMoveTime", actor.mayMoveTime);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("prevMoveDir[i0]", actor.prevMoveDir[i0]);
    }
    v.Field("leanAmount", actor.leanAmount);
    v.Field("isFacingMotion", actor.isFacingMotion);
    v.Field("lastPathToEnemyTime", actor.lastPathToEnemyTime);
    v.Field("bNotifyTurnDone", actor.bNotifyTurnDone);
    v.Field("exposedStartTime", actor.exposedStartTime);
    v.Field("exposedDuration", actor.exposedDuration);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("codeGoal.pos[i0]", actor.codeGoal.pos[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("codeGoal.ang[i0]", actor.codeGoal.ang[i0]);
    }
    v.Field("codeGoal.radius", actor.codeGoal.radius);
    v.Field("codeGoal.height", actor.codeGoal.height);
    v.Field("codeGoalSrc", actor.codeGoalSrc);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("scriptGoal.pos[i0]", actor.scriptGoal.pos[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("scriptGoal.ang[i0]", actor.scriptGoal.ang[i0]);
    }
    v.Field("scriptGoal.radius", actor.scriptGoal.radius);
    v.Field("scriptGoal.height", actor.scriptGoal.height);
    v.Field("pathEnemyLookahead", actor.pathEnemyLookahead);
    v.Field("pathEnemyFightDist", actor.pathEnemyFightDist);
    v.Field("meleeAttackDist", actor.meleeAttackDist);
    v.Field("useEnemyGoal", actor.useEnemyGoal);
    v.Field("useMeleeAttackSpot", actor.useMeleeAttackSpot);
    v.Field("goalPosChanged", actor.goalPosChanged);
    v.Field("commitToFixedNode", actor.commitToFixedNode);
    v.Field("ignoreForFixedNodeSafeCheck", actor.ignoreForFixedNodeSafeCheck);
    v.Field("fixedNode", actor.fixedNode);
    v.Field("fixedNodeSafeRadius", actor.fixedNodeSafeRadius);
    v.Field("fixedNodeSafeVolumeRadiusSq", actor.fixedNodeSafeVolumeRadiusSq);
    v.Field("bPacifist", actor.bPacifist);
    v.Field("iPacifistWait", actor.iPacifistWait);
    v.Field("numCoverNodesInGoal", actor.numCoverNodesInGoal);
    v.Field("iPotentialCoverNodeCount", actor.iPotentialCoverNodeCount);
    v.Field("iPotentialReacquireNodeCount", actor.iPotentialReacquireNodeCount);
    v.Field("engageMinDist", actor.engageMinDist);
    v.Field("engageMinFalloffDist", actor.engageMinFalloffDist);
    v.Field("engageMaxDist", actor.engageMaxDist);
    v.Field("engageMaxFalloffDist", actor.engageMaxFalloffDist);
    v.Field("arrivalInfo.arrivalNotifyRequested", actor.arrivalInfo.arrivalNotifyRequested);
    v.Field("arrivalInfo.animscriptOverrideRunTo", actor.arrivalInfo.animscriptOverrideRunTo);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("arrivalInfo.animscriptOverrideRunToPos[i0]", actor.arrivalInfo.animscriptOverrideRunToPos[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("arrivalInfo.animscriptOverrideOriginError[i0]", actor.arrivalInfo.animscriptOverrideOriginError[i0]);
    }
    v.Field("fovDot", actor.fovDot);
    v.Field("fMaxSightDistSqrd", actor.fMaxSightDistSqrd);
    v.Field("ignoreCloseFoliage", actor.ignoreCloseFoliage);
    for (unsigned int i0 = 0; i0 != 48; ++i0)
    {
        v.Field("sentientInfo[i0].VisCache.bVisible", actor.sentientInfo[i0].VisCache.bVisible);
        v.Field("sentientInfo[i0].VisCache.iLastUpdateTime", actor.sentientInfo[i0].VisCache.iLastUpdateTime);
        v.Field("sentientInfo[i0].VisCache.iLastVisTime", actor.sentientInfo[i0].VisCache.iLastVisTime);
        v.Field("sentientInfo[i0].iLastAttackMeTime", actor.sentientInfo[i0].iLastAttackMeTime);
        v.Field("sentientInfo[i0].lastKnownPosTime", actor.sentientInfo[i0].lastKnownPosTime);
        v.Field("sentientInfo[i0].attackTime", actor.sentientInfo[i0].attackTime);
        for (unsigned int i1 = 0; i1 != 3; ++i1)
        {
            v.Field("sentientInfo[i0].vLastKnownPos[i1]", actor.sentientInfo[i0].vLastKnownPos[i1]);
        }
    }
    v.Field("talkToSpecies", actor.talkToSpecies);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("lastEnemySightPos[i0]", actor.lastEnemySightPos[i0]);
    }
    v.Field("lastEnemySightPosValid", actor.lastEnemySightPosValid);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("anglesToLikelyEnemyPath[i0]", actor.anglesToLikelyEnemyPath[i0]);
    }
    v.Field("faceLikelyEnemyPathNeedCheckTime", actor.faceLikelyEnemyPathNeedCheckTime);
    v.Field("faceLikelyEnemyPathNeedRecalculateTime", actor.faceLikelyEnemyPathNeedRecalculateTime);
    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {
        v.Field("suppression[i0].time", actor.suppression[i0].time);
        for (unsigned int i1 = 0; i1 != 2; ++i1)
        {
            v.Field("suppression[i0].normal[i1]", actor.suppression[i0].normal[i1]);
        }
        v.Field("suppression[i0].dist", actor.suppression[i0].dist);
        v.Field("suppression[i0].friendly", actor.suppression[i0].friendly);
    }
    v.Field("ignoreSuppression", actor.ignoreSuppression);
    v.Field("suppressionWait", actor.suppressionWait);
    v.Field("suppressionDuration", actor.suppressionDuration);
    v.Field("suppressionStartTime", actor.suppressionStartTime);
    v.Field("suppressionMeter", actor.suppressionMeter);
    v.Field("potentialThreat.isEnabled", actor.potentialThreat.isEnabled);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("potentialThreat.direction[i0]", actor.potentialThreat.direction[i0]);
    }
    v.Field("threatUpdateTime", actor.threatUpdateTime);
    v.Field("hasThreateningEnemy", actor.hasThreateningEnemy);
    v.Field("grenadeAwareness", actor.grenadeAwareness);
    v.Field("bThrowbackGrenades", actor.bThrowbackGrenades);
    v.Field("iGrenadeWeaponIndex", actor.iGrenadeWeaponIndex);
    v.Field("bGrenadeTossValid", actor.bGrenadeTossValid);
    v.Field("bGrenadeTargetValid", actor.bGrenadeTargetValid);
    v.Field("iGrenadeAmmo", actor.iGrenadeAmmo);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("vGrenadeTossPos[i0]", actor.vGrenadeTossPos[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("vGrenadeTargetPos[i0]", actor.vGrenadeTargetPos[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("vGrenadeTossVel[i0]", actor.vGrenadeTossVel[i0]);
    }
    v.Field("bDropWeapon", actor.bDropWeapon);
    v.Field("bDrawOnCompass", actor.bDrawOnCompass);
    v.Field("bActivateCrosshair", actor.bActivateCrosshair);
    v.Field("ignoreTriggers", actor.ignoreTriggers);
    v.Field("pushable", actor.pushable);
    v.Field("inuse", actor.inuse);
    v.Field("aiBadPlace", actor.aiBadPlace);
    v.Field("badPlaceAwareness", actor.badPlaceAwareness);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("goodShootPos[i0]", actor.goodShootPos[i0]);
    }
    v.Field("goodShootPosValid", actor.goodShootPosValid);
    v.Field("moveHistoryIndex", actor.moveHistoryIndex);
    for (unsigned int i0 = 0; i0 != 10; ++i0)
    {
        for (unsigned int i1 = 0; i1 != 2; ++i1)
        {
            v.Field("moveHistory[i0][i1]", actor.moveHistory[i0][i1]);
        }
    }
    v.Field("yawVeloc", actor.yawVeloc);
    v.Field("flashBanged", actor.flashBanged);
    v.Field("flashBangedStrength", actor.flashBangedStrength);
    v.Field("flashBangImmunity", actor.flashBangImmunity);
    v.Field("fixedLinkYawOnly", actor.fixedLinkYawOnly);
    v.Field("ikPriority", actor.ikPriority);
    v.Field("allowPitch", actor.allowPitch);
    v.Field("followMin", actor.followMin);
    v.Field("followMax", actor.followMax);
    v.Field("chainFallback", actor.chainFallback);
    v.Field("turretAnimIndex", actor.turretAnimIndex);
    v.Field("hasTurretAnim", actor.hasTurretAnim);
    v.Field("proneAnimLow", actor.proneAnimLow);
    v.Field("proneAnimLevel", actor.proneAnimLevel);
    v.Field("proneAnimHigh", actor.proneAnimHigh);
    v.Field("turnRate", actor.turnRate);
    v.Field("perfectAim", actor.perfectAim);
    v.Field("useable", actor.useable);
    for (unsigned int i0 = 0; i0 != 48; ++i0)
    {
        v.Field("sightTraceHitNum[i0]", actor.sightTraceHitNum[i0]);
    }
    v.Field("AnimScriptSpecific.func", actor.AnimScriptSpecific.func);
