// Named field inventory adapted from campaign-save-wip 2e2ff052.
// Native pointers are never serialized as bytes; objective text is owned data.

    v.Field("ps.commandTime", client.ps.commandTime);
    v.Field("ps.pm_type", client.ps.pm_type);
    v.Field("ps.bobCycle", client.ps.bobCycle);
    v.Field("ps.pm_flags", client.ps.pm_flags);
    v.Field("ps.weapFlags", client.ps.weapFlags);
    v.Field("ps.otherFlags", client.ps.otherFlags);
    v.Field("ps.pm_time", client.ps.pm_time);
    v.Field("ps.loopSoundId", client.ps.loopSoundId);
    v.Field("ps.loopSoundFade", client.ps.loopSoundFade);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("ps.origin[i0]", client.ps.origin[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("ps.velocity[i0]", client.ps.velocity[i0]);
    }
    v.Field("ps.weaponTime", client.ps.weaponTime);
    v.Field("ps.weaponDelay", client.ps.weaponDelay);
    v.Field("ps.weaponTimeLeft", client.ps.weaponTimeLeft);
    v.Field("ps.weaponDelayLeft", client.ps.weaponDelayLeft);
    v.Field("ps.weaponIdleTime", client.ps.weaponIdleTime);
    v.Field("ps.grenadeTimeLeft", client.ps.grenadeTimeLeft);
    v.Field("ps.throwBackGrenadeOwner", client.ps.throwBackGrenadeOwner);
    v.Field("ps.throwBackGrenadeTimeLeft", client.ps.throwBackGrenadeTimeLeft);
    v.Field("ps.weaponRestrictKickTime", client.ps.weaponRestrictKickTime);
    v.Field("ps.mountAvailable", client.ps.mountAvailable);
    v.Field("ps.bRunLeftGun", client.ps.bRunLeftGun);
    v.Field("ps.bCarryingTurret", client.ps.bCarryingTurret);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("ps.mountPos[i0]", client.ps.mountPos[i0]);
    }
    v.Field("ps.mountDir", client.ps.mountDir);
    v.Field("ps.foliageSoundTime", client.ps.foliageSoundTime);
    v.Field("ps.gravity", client.ps.gravity);
    v.Field("ps.leanf", client.ps.leanf);
    v.Field("ps.speed", client.ps.speed);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("ps.delta_angles[i0]", client.ps.delta_angles[i0]);
    }
    v.Field("ps.groundEntityNum", client.ps.groundEntityNum);
    v.Field("ps.groundType", client.ps.groundType);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("ps.vLadderVec[i0]", client.ps.vLadderVec[i0]);
    }
    v.Field("ps.jumpTime", client.ps.jumpTime);
    v.Field("ps.jumpOriginZ", client.ps.jumpOriginZ);
    v.Field("ps.slideTime", client.ps.slideTime);
    v.Field("ps.moveType", client.ps.moveType);
    v.Field("ps.legsTimer", client.ps.legsTimer);
    v.Field("ps.torsoTimer", client.ps.torsoTimer);
    v.Field("ps.legsAnim", client.ps.legsAnim);
    v.Field("ps.torsoAnim", client.ps.torsoAnim);
    v.Field("ps.legsAnimDuration", client.ps.legsAnimDuration);
    v.Field("ps.torsoAnimDuration", client.ps.torsoAnimDuration);
    v.Field("ps.damageTimer", client.ps.damageTimer);
    v.Field("ps.damageDuration", client.ps.damageDuration);
    v.Field("ps.dmgDirection", client.ps.dmgDirection);
    v.Field("ps.dmgType", client.ps.dmgType);
    v.Field("ps.corpseIndex", client.ps.corpseIndex);
    v.Field("ps.movementDir", client.ps.movementDir);
    v.Field("ps.eFlags", client.ps.eFlags);
    v.Field("ps.eFlags2", client.ps.eFlags2);


    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {

    }
    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {

    }


    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {

    }
    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {

    }
    v.Field("ps.clientNum", client.ps.clientNum);
    v.Field("ps.offHandIndex", client.ps.offHandIndex);
    v.Field("ps.offhandSecondary", client.ps.offhandSecondary);
    v.Field("ps.offhandPrimary", client.ps.offhandPrimary);
    v.Field("ps.renderOptions.i", client.ps.renderOptions.i);
    v.Field("ps.weapon", client.ps.weapon);
    v.Field("ps.lastStandPrevWeapon", client.ps.lastStandPrevWeapon);
    v.Field("ps.lastWeaponAltModeSwitch", client.ps.lastWeaponAltModeSwitch);
    v.Field("ps.stowedWeapon", client.ps.stowedWeapon);
    v.Field("ps.stowedWeaponCamo", client.ps.stowedWeaponCamo);
    v.Field("ps.meleeWeapon", client.ps.meleeWeapon);
    v.Field("ps.weaponstate", client.ps.weaponstate);
    v.Field("ps.weaponstateLeft", client.ps.weaponstateLeft);
    v.Field("ps.weaponShotCount", client.ps.weaponShotCount);
    v.Field("ps.weaponShotCountLeft", client.ps.weaponShotCountLeft);
    v.Field("ps.fWeaponPosFrac", client.ps.fWeaponPosFrac);
    v.Field("ps.adsDelayTime", client.ps.adsDelayTime);
    v.Field("ps.spreadOverride", client.ps.spreadOverride);
    v.Field("ps.spreadOverrideState", client.ps.spreadOverrideState);
    v.Field("ps.weaponSpinLerp", client.ps.weaponSpinLerp);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("ps.viewangles[i0]", client.ps.viewangles[i0]);
    }
    v.Field("ps.viewHeightTarget", client.ps.viewHeightTarget);
    v.Field("ps.viewHeightCurrent", client.ps.viewHeightCurrent);
    v.Field("ps.viewHeightLerpTime", client.ps.viewHeightLerpTime);
    v.Field("ps.viewHeightLerpTarget", client.ps.viewHeightLerpTarget);
    v.Field("ps.viewHeightLerpDown", client.ps.viewHeightLerpDown);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("ps.viewAngleClampBase[i0]", client.ps.viewAngleClampBase[i0]);
    }
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("ps.viewAngleClampRange[i0]", client.ps.viewAngleClampRange[i0]);
    }
    v.Field("ps.stackFireCount", client.ps.stackFireCount);
    v.Field("ps.damageEvent", client.ps.damageEvent);
    v.Field("ps.damageYaw", client.ps.damageYaw);
    v.Field("ps.damagePitch", client.ps.damagePitch);
    v.Field("ps.damageCount", client.ps.damageCount);
    for (unsigned int i0 = 0; i0 != 5; ++i0)
    {
        v.Field("ps.stats[i0]", client.ps.stats[i0]);
    }
    for (unsigned int i0 = 0; i0 != 15; ++i0)
    {
        v.Field("ps.heldWeapons[i0].weapon", client.ps.heldWeapons[i0].weapon);
        v.Field("ps.heldWeapons[i0].model", client.ps.heldWeapons[i0].model);
        v.Field("ps.heldWeapons[i0].options.i", client.ps.heldWeapons[i0].options.i);
        v.Field("ps.heldWeapons[i0].heatPercent", client.ps.heldWeapons[i0].heatPercent);
        v.Field("ps.heldWeapons[i0].fuelTankTime", client.ps.heldWeapons[i0].fuelTankTime);
        v.Field("ps.heldWeapons[i0].overHeating", client.ps.heldWeapons[i0].overHeating);
        v.Field("ps.heldWeapons[i0].needsRechamber", client.ps.heldWeapons[i0].needsRechamber);
        v.Field("ps.heldWeapons[i0].heldBefore", client.ps.heldWeapons[i0].heldBefore);
        v.Field("ps.heldWeapons[i0].quickReload", client.ps.heldWeapons[i0].quickReload);
        v.Field("ps.heldWeapons[i0].blockWeaponPickup", client.ps.heldWeapons[i0].blockWeaponPickup);
    }
    for (unsigned int i0 = 0; i0 != 15; ++i0)
    {
        v.Field("ps.ammoNotInClip[i0].ammoIndex", client.ps.ammoNotInClip[i0].ammoIndex);
        v.Field("ps.ammoNotInClip[i0].count", client.ps.ammoNotInClip[i0].count);
    }
    for (unsigned int i0 = 0; i0 != 15; ++i0)
    {
        v.Field("ps.ammoInClip[i0].clipIndex", client.ps.ammoInClip[i0].clipIndex);
        v.Field("ps.ammoInClip[i0].count", client.ps.ammoInClip[i0].count);
    }
    v.Field("ps.proneDirection", client.ps.proneDirection);
    v.Field("ps.proneDirectionPitch", client.ps.proneDirectionPitch);
    v.Field("ps.proneTorsoPitch", client.ps.proneTorsoPitch);
    v.Field("ps.viewlocked", client.ps.viewlocked);
    v.Field("ps.viewlocked_entNum", client.ps.viewlocked_entNum);
    v.Field("ps.vehiclePos", client.ps.vehiclePos);
    v.Field("ps.vehicleType", client.ps.vehicleType);
    v.Field("ps.vehicleAnimBoneIndex", client.ps.vehicleAnimBoneIndex);
    v.Field("ps.linkFlags", client.ps.linkFlags);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("ps.linkAngles[i0]", client.ps.linkAngles[i0]);
    }
    v.Field("ps.cursorHint", client.ps.cursorHint);
    v.Field("ps.cursorHintString", client.ps.cursorHintString);
    v.Field("ps.cursorHintEntIndex", client.ps.cursorHintEntIndex);
    v.Field("ps.iCompassPlayerInfo", client.ps.iCompassPlayerInfo);
    v.Field("ps.spyplaneTypeEnabled", client.ps.spyplaneTypeEnabled);
    v.Field("ps.satelliteTypeEnabled", client.ps.satelliteTypeEnabled);
    v.Field("ps.locationSelectionType", client.ps.locationSelectionType);
    v.Field("ps.locationSelectionInfo", client.ps.locationSelectionInfo);
    v.Field("ps.sprintState.sprintButtonUpRequired", client.ps.sprintState.sprintButtonUpRequired);
    v.Field("ps.sprintState.sprintDelay", client.ps.sprintState.sprintDelay);
    v.Field("ps.sprintState.lastSprintStart", client.ps.sprintState.lastSprintStart);
    v.Field("ps.sprintState.lastSprintEnd", client.ps.sprintState.lastSprintEnd);
    v.Field("ps.sprintState.sprintStartMaxLength", client.ps.sprintState.sprintStartMaxLength);
    v.Field("ps.sprintState.sprintDuration", client.ps.sprintState.sprintDuration);
    v.Field("ps.sprintState.sprintCooldown", client.ps.sprintState.sprintCooldown);
    v.Field("ps.lastDtpEnd", client.ps.lastDtpEnd);
    v.Field("ps.fTorsoPitch", client.ps.fTorsoPitch);
    v.Field("ps.fWaistPitch", client.ps.fWaistPitch);
    v.Field("ps.holdBreathScale", client.ps.holdBreathScale);
    v.Field("ps.holdBreathTimer", client.ps.holdBreathTimer);
    v.Field("ps.moveSpeedScaleMultiplier", client.ps.moveSpeedScaleMultiplier);
    v.Field("ps.mantleState.yaw", client.ps.mantleState.yaw);
    v.Field("ps.mantleState.timer", client.ps.mantleState.timer);
    v.Field("ps.mantleState.transIndex", client.ps.mantleState.transIndex);
    v.Field("ps.mantleState.flags", client.ps.mantleState.flags);
    v.Field("ps.vehicleAnimStage", client.ps.vehicleAnimStage);
    v.Field("ps.vehicleEntryPoint", client.ps.vehicleEntryPoint);
    v.Field("ps.scriptedAnim", client.ps.scriptedAnim);
    v.Field("ps.scriptedAnimTime", client.ps.scriptedAnimTime);
    v.Field("ps.meleeChargeYaw", client.ps.meleeChargeYaw);
    v.Field("ps.meleeChargeDist", client.ps.meleeChargeDist);
    v.Field("ps.meleeChargeTime", client.ps.meleeChargeTime);
    v.Field("ps.weapLockFlags", client.ps.weapLockFlags);
    v.Field("ps.weapLockedEntnum", client.ps.weapLockedEntnum);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("ps.perks[i0]", client.ps.perks[i0]);
    }
    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {
        v.Field("ps.actionSlotType[i0]", client.ps.actionSlotType[i0]);
    }
    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {
        v.Field("ps.actionSlotParam[i0].specifyWeapon.index", client.ps.actionSlotParam[i0].specifyWeapon.index);
    }

    v.Field("ps.weapAnim", client.ps.weapAnim);
    v.Field("ps.weapAnimLeft", client.ps.weapAnimLeft);
    v.Field("ps.aimSpreadScale", client.ps.aimSpreadScale);
    v.Field("ps.shellshockIndex", client.ps.shellshockIndex);
    v.Field("ps.shellshockTime", client.ps.shellshockTime);
    v.Field("ps.shellshockDuration", client.ps.shellshockDuration);
    v.Field("ps.dofNearStart", client.ps.dofNearStart);
    v.Field("ps.dofNearEnd", client.ps.dofNearEnd);
    v.Field("ps.dofFarStart", client.ps.dofFarStart);
    v.Field("ps.dofFarEnd", client.ps.dofFarEnd);
    v.Field("ps.dofNearBlur", client.ps.dofNearBlur);
    v.Field("ps.dofFarBlur", client.ps.dofFarBlur);
    v.Field("ps.dofViewmodelStart", client.ps.dofViewmodelStart);
    v.Field("ps.dofViewmodelEnd", client.ps.dofViewmodelEnd);
    v.Field("ps.waterlevel", client.ps.waterlevel);
    v.Field("ps.smokeColorIndex", client.ps.smokeColorIndex);
    v.Field("ps.hudElemLastAssignedSoundID", client.ps.hudElemLastAssignedSoundID);
    v.Field("ps.adsZoomSelect", client.ps.adsZoomSelect);
    v.Field("ps.adsZoomLatchTime", client.ps.adsZoomLatchTime);
    v.Field("ps.adsZoomLatchState", client.ps.adsZoomLatchState);
    v.Field("ps.artilleryInboundIconLocation", client.ps.artilleryInboundIconLocation);
    v.Field("ps.visionSetLerpRatio", client.ps.visionSetLerpRatio);
    v.Field("ps.poisoned", client.ps.poisoned);
    v.Field("ps.binoculars", client.ps.binoculars);
    v.Field("ps.scriptCursorHintString", client.ps.scriptCursorHintString);
    for (unsigned int i0 = 0; i0 != 32; ++i0)
    {
        v.Field("ps.objective[i0].state", client.ps.objective[i0].state);
        for (unsigned int i1 = 0; i1 != 3; ++i1)
        {
            v.Field("ps.objective[i0].origin[i1]", client.ps.objective[i0].origin[i1]);
        }
        for (unsigned int i1 = 0; i1 != 2; ++i1)
        {
            v.Field("ps.objective[i0].size[i1]", client.ps.objective[i0].size[i1]);
        }
        v.Field("ps.objective[i0].entNum", client.ps.objective[i0].entNum);
        v.Field("ps.objective[i0].teamNum", client.ps.objective[i0].teamNum);
        v.Field("ps.objective[i0].icon", client.ps.objective[i0].icon);
        v.Field("ps.objective[i0].use3D", client.ps.objective[i0].use3D);
        v.Field("ps.objective[i0].color", client.ps.objective[i0].color);
    }
    v.Field("ps.deltaTime", client.ps.deltaTime);
    v.Field("ps.killCamEntity", client.ps.killCamEntity);
    v.Field("ps.killCamTargetEntity", client.ps.killCamTargetEntity);
    for (unsigned int i0 = 0; i0 != 31; ++i0)
    {
        v.Field("ps.hud.current[i0].x", client.ps.hud.current[i0].x);
        v.Field("ps.hud.current[i0].y", client.ps.hud.current[i0].y);
        v.Field("ps.hud.current[i0].z", client.ps.hud.current[i0].z);
        v.Field("ps.hud.current[i0].fontScale", client.ps.hud.current[i0].fontScale);
        v.Field("ps.hud.current[i0].color.rgba", client.ps.hud.current[i0].color.rgba);
        v.Field("ps.hud.current[i0].fromColor.rgba", client.ps.hud.current[i0].fromColor.rgba);
        v.Field("ps.hud.current[i0].fadeStartTime", client.ps.hud.current[i0].fadeStartTime);
        v.Field("ps.hud.current[i0].scaleStartTime", client.ps.hud.current[i0].scaleStartTime);
        v.Field("ps.hud.current[i0].fromX", client.ps.hud.current[i0].fromX);
        v.Field("ps.hud.current[i0].fromY", client.ps.hud.current[i0].fromY);
        v.Field("ps.hud.current[i0].moveStartTime", client.ps.hud.current[i0].moveStartTime);
        v.Field("ps.hud.current[i0].time", client.ps.hud.current[i0].time);
        v.Field("ps.hud.current[i0].duration", client.ps.hud.current[i0].duration);
        v.Field("ps.hud.current[i0].value", client.ps.hud.current[i0].value);
        v.Field("ps.hud.current[i0].sort", client.ps.hud.current[i0].sort);
        v.Field("ps.hud.current[i0].glowColor.rgba", client.ps.hud.current[i0].glowColor.rgba);
        v.Field("ps.hud.current[i0].fxBirthTime", client.ps.hud.current[i0].fxBirthTime);
        v.Field("ps.hud.current[i0].targetEntNum", client.ps.hud.current[i0].targetEntNum);
        v.Field("ps.hud.current[i0].fadeTime", client.ps.hud.current[i0].fadeTime);
        v.Field("ps.hud.current[i0].label", client.ps.hud.current[i0].label);
        v.Field("ps.hud.current[i0].width", client.ps.hud.current[i0].width);
        v.Field("ps.hud.current[i0].height", client.ps.hud.current[i0].height);
        v.Field("ps.hud.current[i0].fromWidth", client.ps.hud.current[i0].fromWidth);
        v.Field("ps.hud.current[i0].fromHeight", client.ps.hud.current[i0].fromHeight);
        v.Field("ps.hud.current[i0].scaleTime", client.ps.hud.current[i0].scaleTime);
        v.Field("ps.hud.current[i0].moveTime", client.ps.hud.current[i0].moveTime);
        v.Field("ps.hud.current[i0].text", client.ps.hud.current[i0].text);
        v.Field("ps.hud.current[i0].fxLetterTime", client.ps.hud.current[i0].fxLetterTime);
        v.Field("ps.hud.current[i0].fxDecayStartTime", client.ps.hud.current[i0].fxDecayStartTime);
        v.Field("ps.hud.current[i0].fxDecayDuration", client.ps.hud.current[i0].fxDecayDuration);
        v.Field("ps.hud.current[i0].fxRedactDecayStartTime", client.ps.hud.current[i0].fxRedactDecayStartTime);
        v.Field("ps.hud.current[i0].fxRedactDecayDuration", client.ps.hud.current[i0].fxRedactDecayDuration);
        v.Field("ps.hud.current[i0].flags", client.ps.hud.current[i0].flags);
        v.Field("ps.hud.current[i0].type", client.ps.hud.current[i0].type);
        v.Field("ps.hud.current[i0].font", client.ps.hud.current[i0].font);
        v.Field("ps.hud.current[i0].alignOrg", client.ps.hud.current[i0].alignOrg);
        v.Field("ps.hud.current[i0].alignScreen", client.ps.hud.current[i0].alignScreen);
        v.Field("ps.hud.current[i0].materialIndex", client.ps.hud.current[i0].materialIndex);
        v.Field("ps.hud.current[i0].offscreenMaterialIdx", client.ps.hud.current[i0].offscreenMaterialIdx);
        v.Field("ps.hud.current[i0].fromAlignOrg", client.ps.hud.current[i0].fromAlignOrg);
        v.Field("ps.hud.current[i0].fromAlignScreen", client.ps.hud.current[i0].fromAlignScreen);
        v.Field("ps.hud.current[i0].soundID", client.ps.hud.current[i0].soundID);
        v.Field("ps.hud.current[i0].ui3dWindow", client.ps.hud.current[i0].ui3dWindow);
        v.Field("ps.hud.current[i0].fromFontScale", client.ps.hud.current[i0].fromFontScale);
        v.Field("ps.hud.current[i0].fontScaleStartTime", client.ps.hud.current[i0].fontScaleStartTime);
        v.Field("ps.hud.current[i0].fontScaleTime", client.ps.hud.current[i0].fontScaleTime);
    }
    for (unsigned int i0 = 0; i0 != 31; ++i0)
    {
        v.Field("ps.hud.archival[i0].x", client.ps.hud.archival[i0].x);
        v.Field("ps.hud.archival[i0].y", client.ps.hud.archival[i0].y);
        v.Field("ps.hud.archival[i0].z", client.ps.hud.archival[i0].z);
        v.Field("ps.hud.archival[i0].fontScale", client.ps.hud.archival[i0].fontScale);
        v.Field("ps.hud.archival[i0].color.rgba", client.ps.hud.archival[i0].color.rgba);
        v.Field("ps.hud.archival[i0].fromColor.rgba", client.ps.hud.archival[i0].fromColor.rgba);
        v.Field("ps.hud.archival[i0].fadeStartTime", client.ps.hud.archival[i0].fadeStartTime);
        v.Field("ps.hud.archival[i0].scaleStartTime", client.ps.hud.archival[i0].scaleStartTime);
        v.Field("ps.hud.archival[i0].fromX", client.ps.hud.archival[i0].fromX);
        v.Field("ps.hud.archival[i0].fromY", client.ps.hud.archival[i0].fromY);
        v.Field("ps.hud.archival[i0].moveStartTime", client.ps.hud.archival[i0].moveStartTime);
        v.Field("ps.hud.archival[i0].time", client.ps.hud.archival[i0].time);
        v.Field("ps.hud.archival[i0].duration", client.ps.hud.archival[i0].duration);
        v.Field("ps.hud.archival[i0].value", client.ps.hud.archival[i0].value);
        v.Field("ps.hud.archival[i0].sort", client.ps.hud.archival[i0].sort);
        v.Field("ps.hud.archival[i0].glowColor.rgba", client.ps.hud.archival[i0].glowColor.rgba);
        v.Field("ps.hud.archival[i0].fxBirthTime", client.ps.hud.archival[i0].fxBirthTime);
        v.Field("ps.hud.archival[i0].targetEntNum", client.ps.hud.archival[i0].targetEntNum);
        v.Field("ps.hud.archival[i0].fadeTime", client.ps.hud.archival[i0].fadeTime);
        v.Field("ps.hud.archival[i0].label", client.ps.hud.archival[i0].label);
        v.Field("ps.hud.archival[i0].width", client.ps.hud.archival[i0].width);
        v.Field("ps.hud.archival[i0].height", client.ps.hud.archival[i0].height);
        v.Field("ps.hud.archival[i0].fromWidth", client.ps.hud.archival[i0].fromWidth);
        v.Field("ps.hud.archival[i0].fromHeight", client.ps.hud.archival[i0].fromHeight);
        v.Field("ps.hud.archival[i0].scaleTime", client.ps.hud.archival[i0].scaleTime);
        v.Field("ps.hud.archival[i0].moveTime", client.ps.hud.archival[i0].moveTime);
        v.Field("ps.hud.archival[i0].text", client.ps.hud.archival[i0].text);
        v.Field("ps.hud.archival[i0].fxLetterTime", client.ps.hud.archival[i0].fxLetterTime);
        v.Field("ps.hud.archival[i0].fxDecayStartTime", client.ps.hud.archival[i0].fxDecayStartTime);
        v.Field("ps.hud.archival[i0].fxDecayDuration", client.ps.hud.archival[i0].fxDecayDuration);
        v.Field("ps.hud.archival[i0].fxRedactDecayStartTime", client.ps.hud.archival[i0].fxRedactDecayStartTime);
        v.Field("ps.hud.archival[i0].fxRedactDecayDuration", client.ps.hud.archival[i0].fxRedactDecayDuration);
        v.Field("ps.hud.archival[i0].flags", client.ps.hud.archival[i0].flags);
        v.Field("ps.hud.archival[i0].type", client.ps.hud.archival[i0].type);
        v.Field("ps.hud.archival[i0].font", client.ps.hud.archival[i0].font);
        v.Field("ps.hud.archival[i0].alignOrg", client.ps.hud.archival[i0].alignOrg);
        v.Field("ps.hud.archival[i0].alignScreen", client.ps.hud.archival[i0].alignScreen);
        v.Field("ps.hud.archival[i0].materialIndex", client.ps.hud.archival[i0].materialIndex);
        v.Field("ps.hud.archival[i0].offscreenMaterialIdx", client.ps.hud.archival[i0].offscreenMaterialIdx);
        v.Field("ps.hud.archival[i0].fromAlignOrg", client.ps.hud.archival[i0].fromAlignOrg);
        v.Field("ps.hud.archival[i0].fromAlignScreen", client.ps.hud.archival[i0].fromAlignScreen);
        v.Field("ps.hud.archival[i0].soundID", client.ps.hud.archival[i0].soundID);
        v.Field("ps.hud.archival[i0].ui3dWindow", client.ps.hud.archival[i0].ui3dWindow);
        v.Field("ps.hud.archival[i0].fromFontScale", client.ps.hud.archival[i0].fromFontScale);
        v.Field("ps.hud.archival[i0].fontScaleStartTime", client.ps.hud.archival[i0].fontScaleStartTime);
        v.Field("ps.hud.archival[i0].fontScaleTime", client.ps.hud.archival[i0].fontScaleTime);
    }
    v.Field("sess.sessionState", client.sess.sessionState);
    v.Field("sess.forceSpectatorClient", client.sess.forceSpectatorClient);
    v.Field("sess.killCamEntity", client.sess.killCamEntity);
    v.Field("sess.killCamTargetEntity", client.sess.killCamTargetEntity);
    v.Field("sess.archiveTime", client.sess.archiveTime);
    v.ScriptObject("sess.scriptPersId", client.sess.scriptPersId);
    v.Field("sess.connected", client.sess.connected);
    v.Field("sess.cmd.serverTime", client.sess.cmd.serverTime);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("sess.cmd.button_bits.array[i0]", client.sess.cmd.button_bits.array[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("sess.cmd.angles[i0]", client.sess.cmd.angles[i0]);
    }
    v.Field("sess.cmd.weapon", client.sess.cmd.weapon);
    v.Field("sess.cmd.offHandIndex", client.sess.cmd.offHandIndex);
    v.Field("sess.cmd.lastWeaponAltModeSwitch", client.sess.cmd.lastWeaponAltModeSwitch);
    v.Field("sess.cmd.forwardmove", client.sess.cmd.forwardmove);
    v.Field("sess.cmd.rightmove", client.sess.cmd.rightmove);
    v.Field("sess.cmd.upmove", client.sess.cmd.upmove);
    v.Field("sess.cmd.pitchmove", client.sess.cmd.pitchmove);
    v.Field("sess.cmd.yawmove", client.sess.cmd.yawmove);
    v.Field("sess.cmd.meleeChargeYaw", client.sess.cmd.meleeChargeYaw);
    v.Field("sess.cmd.meleeChargeDist", client.sess.cmd.meleeChargeDist);
    v.Field("sess.cmd.rollmove", client.sess.cmd.rollmove);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("sess.cmd.selectedLocation[i0]", client.sess.cmd.selectedLocation[i0]);
    }
    v.Field("sess.cmd.selectedYaw", client.sess.cmd.selectedYaw);
    v.Field("sess.cmd.gunPitch", client.sess.cmd.gunPitch);
    v.Field("sess.cmd.gunYaw", client.sess.cmd.gunYaw);
    v.Field("sess.oldcmd.serverTime", client.sess.oldcmd.serverTime);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("sess.oldcmd.button_bits.array[i0]", client.sess.oldcmd.button_bits.array[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("sess.oldcmd.angles[i0]", client.sess.oldcmd.angles[i0]);
    }
    v.Field("sess.oldcmd.weapon", client.sess.oldcmd.weapon);
    v.Field("sess.oldcmd.offHandIndex", client.sess.oldcmd.offHandIndex);
    v.Field("sess.oldcmd.lastWeaponAltModeSwitch", client.sess.oldcmd.lastWeaponAltModeSwitch);
    v.Field("sess.oldcmd.forwardmove", client.sess.oldcmd.forwardmove);
    v.Field("sess.oldcmd.rightmove", client.sess.oldcmd.rightmove);
    v.Field("sess.oldcmd.upmove", client.sess.oldcmd.upmove);
    v.Field("sess.oldcmd.pitchmove", client.sess.oldcmd.pitchmove);
    v.Field("sess.oldcmd.yawmove", client.sess.oldcmd.yawmove);
    v.Field("sess.oldcmd.meleeChargeYaw", client.sess.oldcmd.meleeChargeYaw);
    v.Field("sess.oldcmd.meleeChargeDist", client.sess.oldcmd.meleeChargeDist);
    v.Field("sess.oldcmd.rollmove", client.sess.oldcmd.rollmove);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("sess.oldcmd.selectedLocation[i0]", client.sess.oldcmd.selectedLocation[i0]);
    }
    v.Field("sess.oldcmd.selectedYaw", client.sess.oldcmd.selectedYaw);
    v.Field("sess.oldcmd.gunPitch", client.sess.oldcmd.gunPitch);
    v.Field("sess.oldcmd.gunYaw", client.sess.oldcmd.gunYaw);
    v.Field("sess.localClient", client.sess.localClient);
    v.Field("sess.predictItemPickup", client.sess.predictItemPickup);
    for (unsigned int i0 = 0; i0 != 32; ++i0)
    {
        v.Field("sess.newnetname[i0]", client.sess.newnetname[i0]);
    }
    v.Field("sess.maxHealth", client.sess.maxHealth);
    v.Field("sess.enterTime", client.sess.enterTime);
    v.Field("sess.teamState.location", client.sess.teamState.location);
    v.Field("sess.voteCount", client.sess.voteCount);
    v.Field("sess.teamVoteCount", client.sess.teamVoteCount);
    v.Field("sess.moveSpeedScaleMultiplier", client.sess.moveSpeedScaleMultiplier);
    v.Field("sess.viewmodelIndex", client.sess.viewmodelIndex);
    v.Field("sess.noSpectate", client.sess.noSpectate);
    v.Field("sess.teamInfo", client.sess.teamInfo);
    v.Field("sess.cs.clientIndex", client.sess.cs.clientIndex);
    v.Field("sess.cs.team", client.sess.cs.team);
    v.Field("sess.cs.ffaTeam", client.sess.cs.ffaTeam);
    v.Field("sess.cs.modelindex", client.sess.cs.modelindex);
    for (unsigned int i0 = 0; i0 != 6; ++i0)
    {
        v.Field("sess.cs.attachModelIndex[i0]", client.sess.cs.attachModelIndex[i0]);
    }
    for (unsigned int i0 = 0; i0 != 6; ++i0)
    {
        v.Field("sess.cs.attachTagIndex[i0]", client.sess.cs.attachTagIndex[i0]);
    }
    for (unsigned int i0 = 0; i0 != 32; ++i0)
    {
        v.Field("sess.cs.name[i0]", client.sess.cs.name[i0]);
    }
    v.Field("sess.cs.maxSprintTimeMultiplier", client.sess.cs.maxSprintTimeMultiplier);
    v.Field("sess.cs.rank", client.sess.cs.rank);
    v.Field("sess.cs.prestige", client.sess.cs.prestige);
    v.Field("sess.cs.lastDamageTime", client.sess.cs.lastDamageTime);
    v.Field("sess.cs.lastStandStartTime", client.sess.cs.lastStandStartTime);
    v.Field("sess.cs.beingRevived", client.sess.cs.beingRevived);
    v.Field("sess.cs.xuid", client.sess.cs.xuid);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("sess.cs.xuid32[i0]", client.sess.cs.xuid32[i0]);
    }
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("sess.cs.perks[i0]", client.sess.cs.perks[i0]);
    }
    for (unsigned int i0 = 0; i0 != 8; ++i0)
    {
        v.Field("sess.cs.clanAbbrev[i0]", client.sess.cs.clanAbbrev[i0]);
    }
    v.Field("sess.cs.attachedVehEntNum", client.sess.cs.attachedVehEntNum);
    v.Field("sess.cs.attachedVehSeat", client.sess.cs.attachedVehSeat);
    v.Field("sess.cs.needsRevive", client.sess.cs.needsRevive);
    v.Field("sess.cs.vehAnimState", client.sess.cs.vehAnimState);
    v.Field("sess.cs.score.ping", client.sess.cs.score.ping);
    v.Field("sess.cs.score.status_icon", client.sess.cs.score.status_icon);
    v.Field("sess.cs.score.place", client.sess.cs.score.place);
    v.Field("sess.cs.score.score", client.sess.cs.score.score);
    v.Field("sess.cs.score.kills", client.sess.cs.score.kills);
    v.Field("sess.cs.score.assists", client.sess.cs.score.assists);
    v.Field("sess.cs.score.deaths", client.sess.cs.score.deaths);
    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {
        v.Field("sess.cs.score.scoreboardColumns[i0]", client.sess.cs.score.scoreboardColumns[i0]);
    }
    v.Field("sess.cs.clientUIVisibilityFlags", client.sess.cs.clientUIVisibilityFlags);
    v.Field("sess.cs.attachedEntNum", client.sess.cs.attachedEntNum);
    v.Field("sess.cs.attachedTagIndex", client.sess.cs.attachedTagIndex);
    v.Field("sess.psOffsetTime", client.sess.psOffsetTime);
    for (unsigned int i0 = 0; i0 != 18; ++i0)
    {
        v.Field("sess.scoreboardColumnCache[i0]", client.sess.scoreboardColumnCache[i0]);
    }
    v.Field("spectatorClient", client.spectatorClient);
    v.Field("flags", client.flags);
    v.Field("lastCmdTime", client.lastCmdTime);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("button_bits.array[i0]", client.button_bits.array[i0]);
    }
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("oldbutton_bits.array[i0]", client.oldbutton_bits.array[i0]);
    }
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("latched_button_bits.array[i0]", client.latched_button_bits.array[i0]);
    }
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("button_bitsSinceLastFrame.array[i0]", client.button_bitsSinceLastFrame.array[i0]);
    }
    v.Field("fGunPitch", client.fGunPitch);
    v.Field("fGunYaw", client.fGunYaw);
    v.Field("damage_blood", client.damage_blood);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("damage_from[i0]", client.damage_from[i0]);
    }
    v.Field("damage_fromWorld", client.damage_fromWorld);
    v.Field("inactivityTime", client.inactivityTime);
    v.Field("inactivityWarning", client.inactivityWarning);
    v.Field("lastVoiceTime", client.lastVoiceTime);
    v.Field("outWaterTime", client.outWaterTime);
    v.Field("switchSeatTime", client.switchSeatTime);
    v.Field("currentAimSpreadScale", client.currentAimSpreadScale);
    v.Field("dropWeaponTime", client.dropWeaponTime);
    v.Field("sniperRifleFiredTime", client.sniperRifleFiredTime);
    v.Field("sniperRifleMuzzleYaw", client.sniperRifleMuzzleYaw);
    v.Field("PCSpecialPickedUpCount", client.PCSpecialPickedUpCount);
    v.Field("prevLinkAnglesSet", client.prevLinkAnglesSet);
    for (unsigned int i0 = 0; i0 != 4; ++i0)
    {
        v.Field("prevLinkedInvQuat[i0]", client.prevLinkedInvQuat[i0]);
    }
    v.Field("linkAnglesFrac", client.linkAnglesFrac);
    v.Field("linkAnglesLocked", client.linkAnglesLocked);
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("linkAnglesMinClamp[i0]", client.linkAnglesMinClamp[i0]);
    }
    for (unsigned int i0 = 0; i0 != 2; ++i0)
    {
        v.Field("linkAnglesMaxClamp[i0]", client.linkAnglesMaxClamp[i0]);
    }
    v.Field("link_doCollision", client.link_doCollision);
    v.Field("useHoldTime", client.useHoldTime);
    v.Field("useButtonDone", client.useButtonDone);
    v.Field("iLastCompassPlayerInfoEnt", client.iLastCompassPlayerInfoEnt);
    v.Field("compassPingTime", client.compassPingTime);
    v.Field("damageTime", client.damageTime);
    v.Field("v_dmg_roll", client.v_dmg_roll);
    v.Field("v_dmg_pitch", client.v_dmg_pitch);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("swayViewAngles[i0]", client.swayViewAngles[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("swayOffset[i0]", client.swayOffset[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("swayAngles[i0]", client.swayAngles[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("baseAngles[i0]", client.baseAngles[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("baseOrigin[i0]", client.baseOrigin[i0]);
    }
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("recoilAngles[i0]", client.recoilAngles[i0]);
    }
    v.Field("fLastIdleFactor", client.fLastIdleFactor);
    v.Field("weapIdleTime", client.weapIdleTime);
    for (unsigned int i0 = 0; i0 != 3; ++i0)
    {
        v.Field("recoilSpeed[i0]", client.recoilSpeed[i0]);
    }
    v.Field("lastServerTime", client.lastServerTime);
    v.Field("lastSpawnTime", client.lastSpawnTime);
    v.Field("lastWeapon", client.lastWeapon);
    v.Field("previouslyFiring", client.previouslyFiring);
    v.Field("previouslyUsingNightVision", client.previouslyUsingNightVision);
    v.Field("previouslyBeganWeaponRaise", client.previouslyBeganWeaponRaise);
    v.Field("previouslySprinting", client.previouslySprinting);
    v.Field("hasSpyplane", client.hasSpyplane);
    v.Field("hasSatellite", client.hasSatellite);
    v.Field("revive", client.revive);
    v.Field("reviveTime", client.reviveTime);
    v.Field("disallowVehicleUsage", client.disallowVehicleUsage);
    v.Field("lastStand", client.lastStand);
    v.Field("lastStandTime", client.lastStandTime);
    v.Field("deathInvulnerableTime", client.deathInvulnerableTime);
    v.Field("deathInvulnerable", client.deathInvulnerable);
    v.Field("healthShieldEnabled", client.healthShieldEnabled);
    v.Field("autoPickupDisabled", client.autoPickupDisabled);

    v.Field("playerLOSPosition[0]", client.playerLOSPosition[0]);
    v.Field("playerLOSPosition[1]", client.playerLOSPosition[1]);
    v.Field("playerLOSDirection[0]", client.playerLOSDirection[0]);
    v.Field("playerLOSDirection[1]", client.playerLOSDirection[1]);
    v.Field("playerLOSTime", client.playerLOSTime);

    for (unsigned int i = 0; i != 32; ++i)
        v.Text("ps.objective[i].alt_3D_text", client.ps.objective[i].alt_3D_text);
