#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include "singleton.h"
#include "imgui/imgui.h"


inline namespace Configuration
{
	class Settings : public Singleton<Settings>
	{
	public:

		const char* BoxTypes[2] = { "Cornered Box","Full Box" };
		const char* AimTypes[3] = { "Head","Chest","Body"};
		const char* LineTypes[3] = { "Bottom To Enemy","Top To Enemy","Crosshair To Enemy" };
		const char* AimKey[3] = { "XMouse Button 2" , "Left Mouse Button","ALT" };
		const char* config[3] = { "Projectile" , "Riffle Body", "Head"};

		bool b_box2d = false;
		bool b_glow = false;
		bool b_MenuShow = true;
		bool b_Aimbot = true;
		bool b_AimFOV = true;
		bool b_isPredictionAim = false;
		bool b_Smoothing = false;
		bool b_LockWhenClose = true;
		bool b_aimkey = true;
		bool b_config = false;
		bool b_Locked;
		
		

		bool b_Visual = true;
		bool b_EspBox = false;
		bool b_EspName = false;
		bool b_EspHealth = false;
		bool b_EspLine = false;
		

		ImColor BoxVisColor = ImColor(255.f / 255, 0.f, 0.f, 1.f);
		ImColor HealthBarColor = ImColor(0.f, 255.f / 255.f, 0.f, 1.f);
		ImColor LineColor = ImColor(0.f, 0.f, 255.f / 255, 1.f);
		ImColor FovColor = ImColor(255.f / 255, 0.f, 0.f, 1.f);

		float fl_BoxVisColor[4] = { 255.f / 255,0.f,0.f,1.f };//
		float fl_HealthBarColor[4] = { 0.f,255.f / 255,0.f,1.f };//
		float fl_LineColor[4] = { 0,0,255.f / 255,1 };  //
		float fl_FovColor[4] = { 255.f / 255,0.f,0.f,1.f };  //

		float fl_CurrentFOV;
		float fl_SmoothingValue = 0.1f; // from 0-1
		float fl_Speed = 7000.0f;
		float fl_AimFov = 15.f;


		int in_aimkey = 0;
		int in_BoxType = 0;
		int in_LineType = 0;
		int in_CurrentHealth;
		int in_CurrentLoopFrame;
		int in_tab_index = 0;
		int in_aim = 0;
		int in_config = 0;
	};
#define CFG Configuration::Settings::Get()
}
