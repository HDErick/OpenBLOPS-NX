// Named vehicle state; physics/assets/controllers are reconstructed by owner.
template<class V>void NitroFields(V& v,phys_vec3& x){v.Field("vector.x",x.x);v.Field("vector.y",x.y);v.Field("vector.z",x.z);v.Field("vector.w",x.w);}
template<class V>void NitroFields(V& v,phys_mat44& x){NitroFields(v,x.x);NitroFields(v,x.y);NitroFields(v,x.z);NitroFields(v,x.w);}
template<class V>void NitroFields(V& v,usercmd_s& x){v.Field("cmd.time",x.serverTime);v.Field("cmd.buttons",x.button_bits.array);v.Field("cmd.angles",x.angles);v.Field("cmd.weapon",x.weapon);v.Field("cmd.offhand",x.offHandIndex);v.Field("cmd.forward",x.forwardmove);v.Field("cmd.right",x.rightmove);v.Field("cmd.up",x.upmove);v.Field("cmd.lastWeaponAltModeSwitch",x.lastWeaponAltModeSwitch);v.Field("cmd.pitchmove",x.pitchmove);v.Field("cmd.yawmove",x.yawmove);v.Field("cmd.meleeChargeYaw",x.meleeChargeYaw);v.Field("cmd.meleeChargeDist",x.meleeChargeDist);v.Field("cmd.rollmove",x.rollmove);v.Field("cmd.selectedLocation",x.selectedLocation);v.Field("cmd.selectedYaw",x.selectedYaw);v.Field("cmd.gunPitch",x.gunPitch);v.Field("cmd.gunYaw",x.gunYaw);}
template<class V>void NitroFields(V& v,WheelState& x){
v.Field("WheelState.m_state",x.m_state);
v.Field("WheelState.m_last_state",x.m_last_state);
v.Field("WheelState.m_state_time",x.m_state_time);
v.Field("WheelState.m_rate",x.m_rate);
}
template<class V>void NitroFields(V& v,VehicleStun& x){
v.Field("VehicleStun.m_changed_this_frame",x.m_changed_this_frame);
v.Field("VehicleStun.m_stun_time",x.m_stun_time);
v.Field("VehicleStun.m_last_throttle",x.m_last_throttle);
v.Field("VehicleStun.m_throttle_stun_time",x.m_throttle_stun_time);
v.Field("VehicleStun.m_last_brake",x.m_last_brake);
v.Field("VehicleStun.m_brake_stun_time",x.m_brake_stun_time);
}
template<class V>void NitroFields(V& v,NitrousVehicleController& x){
NitroFields(v,x.m_script_goal_position);
v.Field("NitrousVehicleController.m_script_goal_radius",x.m_script_goal_radius);
v.Field("NitrousVehicleController.m_script_goal_speed",x.m_script_goal_speed);
v.Field("NitrousVehicleController.m_drive_path_speed",x.m_drive_path_speed);
v.Field("NitrousVehicleController.m_stopped_time",x.m_stopped_time);
v.Field("NitrousVehicleController.m_stuck_time",x.m_stuck_time);
NitroFields(v,x.m_stuck_position);
NitroFields(v,x.m_cmd);
v.Field("NitrousVehicleController.m_cmd_local_client_num",x.m_cmd_local_client_num);
v.Field("NitrousVehicleController.m_cmd_valid",x.m_cmd_valid);
v.Field("NitrousVehicleController.m_stop_at_goal",x.m_stop_at_goal);
v.Field("NitrousVehicleController.m_jump_button_pressed",x.m_jump_button_pressed);
NitroFields(v,x.m_stun);
}
template<class V>void NitroFields(V& v,NitrousVehicle& x){
for(auto& value:x.m_wheel_state)NitroFields(v,value);
for(auto& value:x.m_wheel_orig_relpo)NitroFields(v,value);
v.Field("NitrousVehicle.m_wheel_damage",x.m_wheel_damage);
v.Field("NitrousVehicle.m_wheel_surf_types",x.m_wheel_surf_types);
v.Field("NitrousVehicle.m_throttle",x.m_throttle);
v.Field("NitrousVehicle.m_brake",x.m_brake);
v.Field("NitrousVehicle.m_hand_brake",x.m_hand_brake);
v.Field("NitrousVehicle.m_script_brake",x.m_script_brake);
v.Field("NitrousVehicle.m_steer_factor",x.m_steer_factor);
v.Field("NitrousVehicle.m_forward_vel",x.m_forward_vel);
NitroFields(v,x.m_ground_vel);
v.Field("NitrousVehicle.m_hand_brake_friction_time",x.m_hand_brake_friction_time);
v.Field("NitrousVehicle.m_entnum",x.m_entnum);
v.Field("NitrousVehicle.m_origin",x.m_origin);
v.Field("NitrousVehicle.m_angles",x.m_angles);
v.Field("NitrousVehicle.m_flags",x.m_flags);
v.Field("NitrousVehicle.m_notify_flags",x.m_notify_flags);
v.Field("NitrousVehicle.m_server_notify_flags",x.m_server_notify_flags);
v.Field("NitrousVehicle.m_collision_hitp",x.m_collision_hitp);
v.Field("NitrousVehicle.m_collision_hitn",x.m_collision_hitn);
v.Field("NitrousVehicle.m_collision_intensity",x.m_collision_intensity);
v.Field("NitrousVehicle.m_collision_stype",x.m_collision_stype);
v.Field("NitrousVehicle.m_collision_entnum",x.m_collision_entnum);
NitroFields(v,x.m_prev_rb_mat);
NitroFields(v,x.mVehicleController);
v.Field("NitrousVehicle.m_fake_rpm",x.m_fake_rpm);
v.Field("NitrousVehicle.m_num_colliding_wheels",x.m_num_colliding_wheels);
v.Field("NitrousVehicle.m_current_front_side_fric_scale",x.m_current_front_side_fric_scale);
v.Field("NitrousVehicle.m_current_rear_side_fric_scale",x.m_current_rear_side_fric_scale);
v.Field("NitrousVehicle.m_current_front_fwd_fric_scale",x.m_current_front_fwd_fric_scale);
v.Field("NitrousVehicle.m_current_rear_fwd_fric_scale",x.m_current_rear_fwd_fric_scale);
v.Field("NitrousVehicle.m_stuck_time",x.m_stuck_time);
v.Field("NitrousVehicle.m_lastNetworkTime",x.m_lastNetworkTime);
v.Field("NitrousVehicle.m_lastErrorReductionTime",x.m_lastErrorReductionTime);
v.Field("NitrousVehicle.m_networkErrorOrigin",x.m_networkErrorOrigin);
v.Field("NitrousVehicle.m_networkErrorAngles",x.m_networkErrorAngles);
v.Field("NitrousVehicle.m_trackDistAccumLeftInches",x.m_trackDistAccumLeftInches);
v.Field("NitrousVehicle.m_trackDistAccumRightInches",x.m_trackDistAccumRightInches);
v.Field("NitrousVehicle.m_sfx_state_flags",x.m_sfx_state_flags);
v.Field("NitrousVehicle.m_speed_scale",x.m_speed_scale);
v.Field("NitrousVehicle.m_drivepath_scale",x.m_drivepath_scale);
v.Field("NitrousVehicle.m_boost_start_time",x.m_boost_start_time);
v.Field("NitrousVehicle.m_boost_time_pool",x.m_boost_time_pool);
v.Field("NitrousVehicle.m_throttle_held_down",x.m_throttle_held_down);
v.Field("NitrousVehicle.m_throttle_time_started",x.m_throttle_time_started);
v.Field("NitrousVehicle.m_desired_speed_factor",x.m_desired_speed_factor);
v.Field("NitrousVehicle.m_acceleration_factor",x.m_acceleration_factor);
v.Field("NitrousVehicle.m_power_braking_factor",x.m_power_braking_factor);
v.Field("NitrousVehicle.m_braking_factor",x.m_braking_factor);
v.Field("NitrousVehicle.m_coasting_factor",x.m_coasting_factor);
v.Field("NitrousVehicle.m_reference_wheel_radius",x.m_reference_wheel_radius);
v.Field("NitrousVehicle.m_steer_current_angle",x.m_steer_current_angle);
v.Field("NitrousVehicle.m_steer_max_angle",x.m_steer_max_angle);
v.Field("NitrousVehicle.m_steer_speed",x.m_steer_speed);
NitroFields(v,x.m_steer_front_pt_loc);
v.Field("NitrousVehicle.m_steer_front_back_length",x.m_steer_front_back_length);
v.Field("NitrousVehicle.m_state_flags",x.m_state_flags);
}
