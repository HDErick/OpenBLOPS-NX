// Pointer-free fake-entity fields; runtime physics/FX/render caches are rebuilt.
template<class V>void FakeTrajectory(V& v,trajectory_t& trajectory)
{
    v.Field("trajectory.trType",trajectory.trType);
    v.Field("trajectory.trTime",trajectory.trTime);
    v.Field("trajectory.trDuration",trajectory.trDuration);
    v.Field("trajectory.trBase",trajectory.trBase);
    v.Field("trajectory.trDelta",trajectory.trDelta);
}

template<class V>void FakeLerp(V& v,LerpEntityState& state)
{
    v.Field("state.eFlags",state.eFlags);
    v.Field("state.eFlags2",state.eFlags2);
    FakeTrajectory(v, state.pos);
    FakeTrajectory(v, state.apos);
    v.Field("state.u.anonymous.data",state.u.anonymous.data);
    v.Field("state.useCount",state.useCount);
}

template<class V>void FakePose(V& v,cpose_t& pose)
{
    v.Field("pose.eType",pose.eType);
    v.Field("pose.eTypeUnion",pose.eTypeUnion);
    v.Field("pose.localClientNum",pose.localClientNum);
    v.Field("pose.isRagdoll",pose.isRagdoll);
    v.Field("pose.startBurnTime",pose.startBurnTime);
    v.Field("pose.wetness",pose.wetness);
    v.Field("pose.origin",pose.origin);
    v.Field("pose.angles",pose.angles);
    v.Field("pose.absmin",pose.absmin);
    v.Field("pose.absmax",pose.absmax);
    unsigned int cullIn = pose.cullIn;
    v.Field("cullIn",cullIn);
    pose.cullIn = cullIn;
    switch (pose.eTypeUnion)
    {
    case ET_PLAYER:
    v.Field("pose.player.tag",pose.player.tag);
    v.Field("pose.player.waterHeight",pose.player.waterHeight);
    v.Field("pose.player.nextWaterHeightCheck",pose.player.nextWaterHeightCheck);
    v.Field("pose.player.nextRippleTime",pose.player.nextRippleTime);
        break;
    case ET_MG42:
    v.Field("pose.turret.angles.pitch",pose.turret.angles.pitch);
    v.Field("pose.turret.angles.yaw",pose.turret.angles.yaw);
    v.Field("pose.turret.barrelPitch",pose.turret.barrelPitch);
    v.Field("pose.turret.pivotOffset",pose.turret.pivotOffset);
    v.Field("pose.turret.playerUsing",pose.turret.playerUsing);
    v.Field("pose.turret.firingOffset",pose.turret.firingOffset);
    v.Field("pose.turret.tag_aim",pose.turret.tag_aim);
    v.Field("pose.turret.tag_aim_pivot",pose.turret.tag_aim_pivot);
    v.Field("pose.turret.tag_aim_animated",pose.turret.tag_aim_animated);
    v.Field("pose.turret.tag_flash",pose.turret.tag_flash);
    v.Field("pose.turret.tag_weapon",pose.turret.tag_weapon);
        break;
    case ET_VEHICLE:
    case ET_VEHICLE_COLLMAP:
    case ET_VEHICLE_CORPSE:
    v.Field("pose.vehicle.pitch",pose.vehicle.pitch);
    v.Field("pose.vehicle.yaw",pose.vehicle.yaw);
    v.Field("pose.vehicle.roll",pose.vehicle.roll);
    v.Field("pose.vehicle.barrelPitch",pose.vehicle.barrelPitch);
    v.Field("pose.vehicle.steerYaw",pose.vehicle.steerYaw);
    v.Field("pose.vehicle.steerPitch",pose.vehicle.steerPitch);
    v.Field("pose.vehicle.steerRoll",pose.vehicle.steerRoll);
    v.Field("pose.vehicle.gunnerPitch",pose.vehicle.gunnerPitch);
    v.Field("pose.vehicle.gunnerYaw",pose.vehicle.gunnerYaw);
    v.Field("pose.vehicle.time",pose.vehicle.time);
    v.Field("pose.vehicle.wheelBoneIndex",pose.vehicle.wheelBoneIndex);
    v.Field("pose.vehicle.nitrousWheelRotation",pose.vehicle.nitrousWheelRotation);
    v.Field("pose.vehicle.nitrousWheelYaw",pose.vehicle.nitrousWheelYaw);
    v.Field("pose.vehicle.wheelHeight",pose.vehicle.wheelHeight);
    v.Field("pose.vehicle.tag_body",pose.vehicle.tag_body);
    v.Field("pose.vehicle.tag_turret",pose.vehicle.tag_turret);
    v.Field("pose.vehicle.tag_barrel",pose.vehicle.tag_barrel);
    v.Field("pose.vehicle.tag_barrel_recoil",pose.vehicle.tag_barrel_recoil);
    v.Field("pose.vehicle.tag_gunner_turret",pose.vehicle.tag_gunner_turret);
    v.Field("pose.vehicle.tag_gunner_barrel",pose.vehicle.tag_gunner_barrel);
    v.Field("pose.vehicle.tag_extra_tank_wheels",pose.vehicle.tag_extra_tank_wheels);
    v.Field("pose.vehicle.tag_minigun_spin",pose.vehicle.tag_minigun_spin);
    v.Field("pose.vehicle.minigun_rotation",pose.vehicle.minigun_rotation);
    v.Field("pose.vehicle.extra_wheel_rot_scale",pose.vehicle.extra_wheel_rot_scale);
    v.Field("pose.vehicle.barrelRecoil",pose.vehicle.barrelRecoil);
        break;
    case ET_ACTOR:
    case ET_ACTOR_CORPSE:
    v.Field("pose.actor.proneType",pose.actor.proneType);
    v.Field("pose.actor.pitch",pose.actor.pitch);
    v.Field("pose.actor.roll",pose.actor.roll);
    v.Field("pose.actor.yaw",pose.actor.yaw);
    v.Field("pose.actor.headBone",pose.actor.headBone);
    v.Field("pose.actor.height",pose.actor.height);
        break;
    case ET_FX:
    case ET_LOOP_FX:
    v.Field("pose.fx.triggerTime",pose.fx.triggerTime);
    v.Field("pose.fx.effect",pose.fx.effect);
        break;
    default:
    v.Field("pose.general.contents",pose.general.contents);
    // This byte is shared by the pose union and is only a link flag for
    // client triggers. Restore rebuilds the collision link on the next frame.
    if constexpr(std::is_same_v<V,checkpoint::FieldReader>)pose.general.linked=false;
    v.Field("pose.general.mins",pose.general.mins);
    v.Field("pose.general.maxs",pose.general.maxs);
        break;
    }
    v.Field("pose.constantSet.value",pose.constantSet.value);
    v.Field("pose.constantSet.constantSource",pose.constantSet.constantSource);
    v.Field("pose.constantSet.used",pose.constantSet.used);
}

template<class V>void FakeState(V& v,entityState_s& state)
{
    FakeLerp(v, state.lerp);
    v.Field("state.number",state.number);
    v.Field("state.time2",state.time2);
    v.Field("state.loopSoundId",state.loopSoundId);
    v.Field("state.solid",state.solid);
    v.Field("state.renderOptions.i",state.renderOptions.i);
    v.Field("state.animState.state",state.animState.state);
    v.Field("state.animState.fLeanAmount",state.animState.fLeanAmount);
    v.Field("state.animState.fAimUpDown",state.animState.fAimUpDown);
    v.Field("state.animState.fAimLeftRight",state.animState.fAimLeftRight);
    v.Field("state.un3.item",state.un3.item);
    v.Field("state.partBits",state.partBits);
    v.Field("state.events",state.events);
    v.Field("state.eventParms",state.eventParms);
    v.Field("state.eventParm",state.eventParm);
    v.Field("state.eType",state.eType);
    v.Field("state.groundEntityNum",state.groundEntityNum);
    v.Field("state.index.xmodel",state.index.xmodel);
    v.Field("state.otherEntityNum",state.otherEntityNum);
    v.Field("state.attackerEntityNum",state.attackerEntityNum);
    v.Field("state.enemyModel",state.enemyModel);
    v.Field("state.weapon",state.weapon);
    v.Field("state.lastStandPrevWeapon",state.lastStandPrevWeapon);
    v.StringId("state.targetname",state.targetname);
    v.Field("state.loopSoundFade",state.loopSoundFade);
    v.Field("state.eventSequence",state.eventSequence);
    v.Field("state.surfType",state.surfType);
    v.Field("state.clientNum",state.clientNum);
    v.Field("state.iHeadIcon",state.iHeadIcon);
    v.Field("state.faction.teamAndOwnerIndex",state.faction.teamAndOwnerIndex);
    v.Field("state.weaponModel",state.weaponModel);
    v.Field("state.un1.scale",state.un1.scale);
    v.Field("state.clientLinkInfo.parentEnt",state.clientLinkInfo.parentEnt);
    v.Field("state.clientLinkInfo.tagIndex",state.clientLinkInfo.tagIndex);
    v.Field("state.clientLinkInfo.flags",state.clientLinkInfo.flags);
    v.Field("state.animTreeIndex",state.animTreeIndex);
}

template<class V>void FakeFakeEntity(V& v,fake_centity_s& row)
{
    v.Field("row.flags",row.flags);
    centity_s &cent = row.cent;
    FakePose(v, cent.pose);
    FakeLerp(v, cent.currentState);
    FakeState(v, cent.nextState);
    v.Field("cent.previousEventSequence",cent.previousEventSequence);
    v.Field("cent.miscTime",cent.miscTime);
    v.Field("cent.lastMuzzleFlash",cent.lastMuzzleFlash);
    v.Field("cent.attachModelNames",cent.attachModelNames);
    for(auto& tag:cent.attachTagNames)v.StringId("attachment_tag",tag);
    v.Field("cent.numBulletImpacts",cent.numBulletImpacts);
    v.Field("cent.nextRippleTime",cent.nextRippleTime);
    v.Field("cent.nextSlideFX",cent.nextSlideFX);
    v.Field("cent.fireTime",cent.fireTime);
    v.Field("cent.flagIndex",cent.flagIndex);
    v.Field("cent.flagState",cent.flagState);
    const char* compassMaterial=cent.compassMaterial?cent.compassMaterial->info.name:nullptr;
    v.Text("compass_material",compassMaterial);
    if constexpr(std::is_same_v<V,checkpoint::FieldReader>)cent.compassMaterial=compassMaterial?DB_FindXAssetHeader(ASSET_TYPE_MATERIAL,const_cast<char*>(compassMaterial),false,-1).material:nullptr;
    v.Field("cent.lastTrailTime",cent.lastTrailTime);
    v.Field("cent.clientFlags",cent.clientFlags);
    v.Field("cent.animCommandTime",cent.animCommandTime);
    v.Field("cent.allowPitch",cent.allowPitch);
    v.Field("cent.actorRenderError_SP",cent.actorRenderError_SP);
    v.Field("cent.rootOriginOffset_SP",cent.rootOriginOffset_SP);
    v.Field("cent.rootAnglesOffset_SP",cent.rootAnglesOffset_SP);
}
