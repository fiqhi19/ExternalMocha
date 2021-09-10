

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <random>
#include <stdint.h>
#include <thread>
#include <list>

#include "classes.h"
#include "overlay.h"
#include "Mhyprot/mhyprot.hpp"
#include "cfg.h"

using X2295::D3DOverlay;
using namespace std;

UEngine CurrentUEngine;
ULocalPlayer CurrentLocalPlayer;
APlayerController CurrentController;
APawn CurrentAcknowledgedPawn;
ACamera CurrentCamera;
AWorldInfo CurrentWorldInfo;
APawn CurrentPawnList;
APawn LockedPawn;



bool IsValid(uint64_t adress) 
{
    if (adress == 0 || adress == 0xCCCCCCCCCCCCCCCC) {
        return false;
    }
    return true;
}

void CallAimbot()
{
    if (!IsValid(LockedPawn.data)) return;
    int Health = LockedPawn.GetHealth();
    if (Health > 1)
    {
        FRotator AimRotation = FRotator{ 0,0,0 };
        bool isVisible = LockedPawn.GetMesh().IsVisible(CurrentWorldInfo.GetTimeSeconds());
        if (isVisible)
        {
            FVector TargetLocation = LockedPawn.GetLocation();
            if (CFG.b_isPredictionAim)
            {
                auto currProjectiles = CurrentAcknowledgedPawn.GetWeapon().GetProjectiles();
                if (currProjectiles.Length() > 0)
                {
                    CFG.fl_Speed = currProjectiles.GetById(0).GetSpeed();
                }
                FVector TargetVelocity = LockedPawn.GetVelocity();
                float TravelTime = math::GetDistance(CurrentAcknowledgedPawn.GetLocation(), TargetLocation) / CFG.fl_Speed;
                TargetLocation = {
                    (TargetLocation.X + TargetVelocity.X * TravelTime),
                    (TargetLocation.Y + TargetVelocity.Y * TravelTime),
                     TargetLocation.Z
                };
            }
            FVector RealLocation = CurrentCamera.GetRealLocation();
			if (CFG.in_aim == 0)
			{
				math::AimAtVector(TargetLocation + FVector(0, 0, LockedPawn.GetEyeHeight()), RealLocation, AimRotation);			
			}
			else if (CFG.in_aim == 1)
			{
				math::AimAtVector(TargetLocation + FVector(0, 0, (LockedPawn.GetEyeHeight() / 2)), RealLocation, AimRotation);
			}
            

            AimRotation.Yaw = math::ClampYaw(AimRotation.Yaw);
            AimRotation.Pitch = math::ClampPitch(AimRotation.Pitch);

            if (CFG.b_Smoothing)
            {
                FRotator currentRotation = CurrentController.GetRotation();
                currentRotation.Roll = 0;

                auto diff = currentRotation - AimRotation;

                auto realDiff = diff;
                auto a = math::ClampYaw(currentRotation.Yaw);
                auto b = math::ClampYaw(AimRotation.Yaw);
                const auto Full360 = Const_URotation180 * 2;

                auto dist1 = -(a - b + Full360) % Full360;
                auto dist2 = (b - a + Full360) % Full360;

                auto dist = dist1;
                if (abs(dist2) < abs(dist1)) {
                    dist = dist2;
                }

                auto smoothAmount = CFG.fl_SmoothingValue;

                if (CFG.b_LockWhenClose && abs(dist) + abs(diff.Pitch) < Const_URotation180 / 100) {
                    smoothAmount = 1;
                }

                diff.Yaw = (int)(dist * smoothAmount);
                diff.Pitch = (int)(diff.Pitch * smoothAmount);
                AimRotation = currentRotation + diff;
            }
            AimRotation.Pitch = AimRotation.Pitch;
            CurrentController.SetRotation(AimRotation);
        }
    }
    else
    {
        CFG.b_Locked = false;
        LockedPawn = APawn{};
        return;
    }
}
void RenderVisual()
{
	auto width = D3DOverlay::getWidth();
	auto height = D3DOverlay::getHeight();

	CurrentUEngine = GetUEngine(GameVars.dwProcess_Base);
	CurrentLocalPlayer = CurrentUEngine.GetLocalPlayer();
	CurrentController = CurrentLocalPlayer.GetController();
	CurrentAcknowledgedPawn = CurrentController.GetAcknowledgedPawn();
	CurrentCamera = CurrentController.GetCamera();
	CurrentWorldInfo = CurrentController.GetWorldInfo();
	CurrentPawnList = CurrentWorldInfo.GetPawnList();

	CFG.in_CurrentHealth = CurrentAcknowledgedPawn.GetHealth();
	CFG.fl_CurrentFOV = CurrentCamera.GetDeafultFov() * CurrentController.GetFovMultiplier();

	APawn CurrentPawn = CurrentPawnList;
	int players = 0;
	bool isAimbotActive = CFG.b_Aimbot && GetAsyncKeyState(VK_XBUTTON2);
	if (!IsValid(LockedPawn.data) || !LockedPawn.GetMesh().IsVisible(CurrentWorldInfo.GetTimeSeconds()) || !isAimbotActive) 
	{
		LockedPawn = APawn{};
		CFG.b_Locked = false;
	}
	float ClosestDistance = 999999.0f;

	auto currentTeamIndex = CurrentAcknowledgedPawn.GetPlayerReplicationInfo().GetTeamInfo().GetTeamIndex();

	while (IsValid(CurrentPawn.data))
	{
		APawn nextPawn = CurrentPawn.GetNextPawn();
		if (!IsValid(CurrentPawn.data))
		{
			CurrentPawn = nextPawn;
			continue;
		}

		int Hp = CurrentPawn.GetHealth();
		players++;
		auto repInfo = CurrentPawn.GetPlayerReplicationInfo();
		if (!IsValid(repInfo.data)) {
			CurrentPawn = nextPawn;
			continue;
		}

		auto teamInfo = repInfo.GetTeamInfo();
		if (!IsValid(teamInfo.data)) {
			CurrentPawn = nextPawn;
			continue;
		}

		auto teamIndex = teamInfo.GetTeamIndex();

		if (teamIndex == currentTeamIndex ||
			CurrentPawn.data == CurrentAcknowledgedPawn.data ||
			Hp < 1 || Hp > 10000)
		{
			CurrentPawn = nextPawn;
			continue;
		}

		auto mesh = CurrentPawn.GetMesh();
		if (!IsValid(mesh.data)) {
			CurrentPawn = nextPawn;
			continue;
		}

		FBoxSphereBounds PlayerHitbox = mesh.GetBounds();

		FVector bottom, head, pawnPos;
		Vec2 bottompos, headpos, pos;

		bottom = math::VectorSubtract(PlayerHitbox.Origin, PlayerHitbox.BoxExtent);
		head = math::VectorAdd(PlayerHitbox.Origin, PlayerHitbox.BoxExtent);

		pawnPos = CurrentPawn.GetLocation();

		FVector posHead = pawnPos;
		FRotator Rotation = CurrentController.GetRotation();
		FVector RealLocation = CurrentCamera.GetRealLocation();

		bool isPawnVisible = mesh.IsVisible(CurrentWorldInfo.GetTimeSeconds());
		auto tracerDistance = math::VectorMagnitude(math::VectorSubtract(RealLocation, pawnPos)) / 50.f;

		try
		{
		if (math::W2S(bottom, bottompos, Rotation, RealLocation, CFG.fl_CurrentFOV) &&
			math::W2S(head, headpos, Rotation, RealLocation, CFG.fl_CurrentFOV))
		{

			float box_height = abs(headpos.y - bottompos.y);
			float box_width = box_height * 0.75;

			if (CFG.b_Visual)
			{
				//BOX

				if (CFG.b_EspBox)
				{
					if (CFG.in_BoxType == 0)
					{
						D3DOverlay::DrawCorneredBox(headpos.x - (box_width / 2), headpos.y, box_width, box_height, CFG.BoxVisColor, 1.5f);					
					}
					else if (CFG.in_BoxType == 1)
					{
						D3DOverlay::DrawBox(CFG.BoxVisColor, headpos.x - (box_width / 2), headpos.y, box_width, box_height);

					}
				}

				//HP
				if (CFG.b_EspHealth)
				{

					auto maxHP = CurrentPawn.GetMaxHealth();
					auto procentage = Hp * 100 / maxHP;

					float width = box_width / 10;
					if (width < 2.f) width = 2.;
					if (width > 3) width = 3.;

					D3DOverlay::ProgressBar(headpos.x - (box_width / 2), headpos.y, width, bottompos.y - headpos.y, procentage, 100, CFG.HealthBarColor, true);
				}
				//Name
				if (CFG.b_EspName)
				{
					D3DOverlay::DrawOutlinedText(X2295::Century_Gothic, repInfo.GetName().ToString(), ImVec2(headpos.x, headpos.y - 20), 15, ImColor(255, 255, 255), true);
				}
				//Line
				if (CFG.b_EspLine)
				{

					if (CFG.in_LineType == 0)
					{
						D3DOverlay::DrawLine(ImVec2(static_cast<float>(width / 2), static_cast<float>(height)), ImVec2(bottompos.x, bottompos.y), CFG.LineColor, 1.3f); //LINE FROM BOTTOM SCREEN
					}
					if (CFG.in_LineType == 1)
					{
						D3DOverlay::DrawLine(ImVec2(static_cast<float>(width / 2), 0.f), ImVec2(bottompos.x, bottompos.y), CFG.LineColor, 1); //LINE FROM TOP SCREEN
					}
					if (CFG.in_LineType == 2)
					{
						D3DOverlay::DrawLine(ImVec2(static_cast<float>(width / 2), static_cast<float>(height / 2)), ImVec2(bottompos.x, bottompos.y), CFG.LineColor, 1.3f); //LINE FROM BOTTOM SCREEN
					}

				}
			}
			if (isAimbotActive && isPawnVisible && !CFG.b_Locked)
			{
				Vec2 headPos;
				if (math::W2S(posHead, headPos, Rotation, RealLocation, CFG.fl_CurrentFOV))
				{
					//TODO SETTING
					float ScreenCX = GameVars.ScreenWidth / 2;
					float ScreenCY = GameVars.ScreenHeight / 2;
					float cx = headPos.x;
					float cy = headPos.y;
					float radiusx_ = CFG.fl_AimFov * (ScreenCX / CFG.fl_CurrentFOV);
					float radiusy_ = CFG.fl_AimFov * (ScreenCY / CFG.fl_CurrentFOV);
					float crosshairDistance = math::GetCrosshairDistance(cx, cy, ScreenCX, ScreenCY);
					if (tracerDistance < 30.f || cx >= ScreenCX - radiusx_ && cx <= ScreenCX + radiusx_ && cy >= ScreenCY - radiusy_ && cy <= ScreenCY + radiusy_)
					{
						if (crosshairDistance < ClosestDistance)
						{
							ClosestDistance = crosshairDistance;
							LockedPawn = CurrentPawn;
						}
					}
					CurrentPawn = nextPawn;
					continue;
				}
			}
		}
		CurrentPawn = nextPawn;
	}
	catch (const std::exception&) {
		CurrentPawn = nextPawn;
		continue;
	}
}

if (IsValid(LockedPawn.data))
{
	CFG.b_Locked = true;
	CallAimbot();
}
}

auto Render(FLOAT aWidth, FLOAT aHeight)->VOID
{
	if (GetAsyncKeyState(VK_INSERT) & 1)
		CFG.b_MenuShow = !CFG.b_MenuShow;


	if (CFG.b_MenuShow)
	{
		ImGui::SetNextWindowSize(ImVec2(675, 410)); // 450,426

		ImGui::PushFont(X2295::DefaultFont);
		ImGui::Begin("Mocha Paladins", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

		D3DOverlay::TabButton("Visual", &CFG.in_tab_index, 0, true);
		D3DOverlay::TabButton("Aimbot", &CFG.in_tab_index, 1, false);

		if (CFG.in_tab_index == 0)
		{
			ImGui::Checkbox("Enabled Visual", &CFG.b_Visual);
			if (CFG.b_Visual)
			{
				ImGui::Checkbox("ESP Box", &CFG.b_EspBox);
				if (CFG.b_EspBox)
				{
					ImGui::Combo("ESP Box Type", &CFG.in_BoxType, CFG.BoxTypes, 2);
					if (ImGui::ColorEdit4("Box Color", CFG.fl_BoxVisColor))
					{
						CFG.BoxVisColor = ImColor(CFG.fl_BoxVisColor[0], CFG.fl_BoxVisColor[1], CFG.fl_BoxVisColor[2], CFG.fl_BoxVisColor[3]);
					}
				}
				ImGui::Checkbox("ESP HealthBar", &CFG.b_EspHealth);
				if (CFG.b_EspHealth)
				{
					if (ImGui::ColorEdit4("Health Bar Color", CFG.fl_HealthBarColor))
					{
						CFG.HealthBarColor = ImColor(CFG.fl_HealthBarColor[0], CFG.fl_HealthBarColor[1], CFG.fl_HealthBarColor[2], CFG.fl_HealthBarColor[3]);
					}
				}
				ImGui::Checkbox("ESP Line", &CFG.b_EspLine);
				if (CFG.b_EspLine)
				{
					ImGui::Combo("Line ESP Type", &CFG.in_LineType, CFG.LineTypes, 3);
					if (ImGui::ColorEdit4("Line ESP Color", CFG.fl_LineColor))
					{
						CFG.LineColor = ImColor(CFG.fl_LineColor[0], CFG.fl_LineColor[1], CFG.fl_LineColor[2], CFG.fl_LineColor[3]);
					}
				}
			}
		}
		else if (CFG.in_tab_index == 1)
		{
			ImGui::Checkbox("Enabled Aimbot", &CFG.b_Aimbot);
			if (CFG.b_Aimbot)
			{
				ImGui::Combo("Aim Target", &CFG.in_aim, CFG.AimTypes, 2);
				ImGui::Text("Aimbot Key MOUSE BUTTON 5");
				ImGui::Checkbox("Enabled PredictionAim", &CFG.b_isPredictionAim);
				ImGui::Checkbox("LockWhenClose", &CFG.b_LockWhenClose);
				ImGui::Checkbox("Smoothing", &CFG.b_Smoothing);
				if (CFG.b_Smoothing)
				{
					ImGui::SliderFloat("Smoothing", &CFG.fl_SmoothingValue, 0.0f, 1.0f);
				}
			}
		
		}
		ImGui::PopFont();
		ImGui::End();
	}
	RenderVisual();
}

int main()
{
	SetConsoleTitle("MOCHA PALADINS CHEAT");

	system("sc stop mhyprot2"); // RELOAD DRIVER JUST IN CASE
	system("CLS"); // CLEAR

    GameVars.dwProcessId = GetProcessId(GameVars.dwProcessName);
    if (!GameVars.dwProcessId)
    {
        printf("[!] process \"%s\ was not found\n", GameVars.dwProcessName);
    }
	printf("[+] %s (%d)\n", GameVars.dwProcessName, GameVars.dwProcessId);
	printf("PRESS INSERT TO TOGGLE MENU");

    if (!mhyprot::init())
    {
        printf("[!] failed to initialize vulnerable driver\n");
    }
    if (!mhyprot::driver_impl::driver_init(
        false,
        false 
    ))
    {
        printf("[!] failed to initialize driver properly\n");
        mhyprot::unload();
    }
	GameVars.dwProcess_Base = GetProcessBase(GameVars.dwProcessId);
	if (!GameVars.dwProcess_Base) {
		printf("[!] failed to get baseadress\n");
		mhyprot::unload();
	}

    HWND  gameHWND = getHWND(GameVars.dwProcessId);
    if (IsWindow(gameHWND))
    {
        if (D3DOverlay::InitOverlay(_T("xxxxx"), _T("xxxxxxxxxx")))
        {

			GameVars.ScreenWidth = D3DOverlay::getWidth();
			GameVars.ScreenHeight = D3DOverlay::getHeight();

			ImGuiIO& io = ImGui::GetIO();
			X2295::DefaultFont = io.Fonts->AddFontDefault();
			X2295::Century_Gothic = io.Fonts->AddFontFromFileTTF("Century_Gothic.ttf", 16);
			io.Fonts->Build();

            D3DOverlay::SetUserRender(Render);
            while (D3DOverlay::MsgLoop() && D3DOverlay::AttachWindow(gameHWND)) {};
            D3DOverlay::UninitOverlay();
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class tgjpwgf {
public:
	int hsxsudi;
	double mijftosdmaox;
	bool zmgmwfkx;
	string zdmwaca;
	int niwev;
	tgjpwgf();
	bool gyglgemcmruggdespdmrcux(string imqnwjqu, double qzxfeo, double cjtoqxojmrrwj, int kwwxaluheesrgrb);
	bool xlwxwzeajcssyntbfacqetcun(bool cgahvmcipvngjuk, int tskjfqegi, int kpygpngqlkcum, string qnxzfrrslpu, bool nvqjqgdyv, string evfhpic, int ihewwkuoevtbte);
	double lcbcalvgwvghxqjjkud(string lbfhwkzpozcs, int gnttlax);
	double fbhnjyhsqlvjlsmogyddbgsd(double yctitrqsc, int gpyamtfphzpxyj, double ediqxfnnbu, double xuffm, double umjrmwh, bool svbyskkcogwbc, int ectrdxxy);

protected:
	int kxlpi;

	double ilbxrfhfgnaadb(string shxgw, int dmjlxfmjbogm, bool szugupoy, bool xpqcwlluiccbep);
	bool hoticgcyupduss(double leyzetox);
	int aczrlnruogbhnikmwfewidpv(bool ynuyzhckydx, double bilylrjr, int njcayk, int jxqfdd, string emnfggd, double fgwmgs, int ntbwfhr);
	bool giwzhmqtopmfjeencaxjwmse(string iudghvghkiixy);
	int urmkbtywchoecgpxkdneydyi(double ztandfsacmijl, int uheiavcubiez, int kilhnnlvzr, double zwdqk);
	int xexgetqfywmvqzicyxjtzz(string rlwtinam, string oqmiqfswig, bool pclgxsri, bool zreeyyqpxjuwbc, double heybvnazjql, bool qtbsolypmkagbqh, int zpgmdleiiur, bool dvnonzsseo);
	double eloxvqupzc(double qrlgwuqw, double slbxgceq, bool mcbyiwemd, string dgbkdpzjoaaf, double drbesstb, int meogjzbvz, string mlrdiusqzg);
	void qtwtatthtnm(double hactsnrrfglywuq, bool flutwrxtd, double cyxzzpfvled, string bicmlqsn, int tqxortdg, bool avlofwyk, string hwwhhevwl);

private:
	string iqwlbxdeivlqohd;
	string rdzvouuc;
	int jswqzhxinfq;

	string uwakcenieljvo(bool putawnx, string sdxsgb, string wkwevqaetd, int dqvaufo, bool jleqokcuexv, double ujmym);
	string pspobyyjky(double thxpjkuvxtfxc, bool msoigtqkqqyzxg, int fbjjnejqyxfqrv, string tcvbz, double opvvlltrhftkrcp, int sasaheuntocai, string xsiinxvjjzy, double vuvwfl, string yunshmmpsi, bool ulzaotclqnfgxk);
	void btqtqmvxcpf(bool ucapcjbpgw);
	string pyztcvzbprycpbhas(string rnwkrd, string lfqfrsbbrdagh, bool offmymywfym, int hzgtnncsa, int oozomjxbqhqfo);
	int whtbntrautcdnscqqibxi(int qrllv, bool bkiylaalliygmg, double aucjgbbwvoobpit, bool niuaoylbepw, string bdwusyazumrza, int lpfbtgtrz, bool abjjcq, bool whqetsuz, string tlawuhvywxyuslw);
	string qgfzomwhvopws(string nqoqigsc, int itxxomqifbz, int updnirl);

};


string tgjpwgf::uwakcenieljvo(bool putawnx, string sdxsgb, string wkwevqaetd, int dqvaufo, bool jleqokcuexv, double ujmym) {
	int yxbgzsypnw = 2638;
	int trljxmlknloifgq = 6996;
	int lnouwjpyf = 6069;
	int jkbof = 522;
	if (2638 != 2638) {
		int eocmlin;
		for (eocmlin = 95; eocmlin > 0; eocmlin--) {
			continue;
		}
	}
	if (2638 != 2638) {
		int xkenykxojy;
		for (xkenykxojy = 36; xkenykxojy > 0; xkenykxojy--) {
			continue;
		}
	}
	if (6069 == 6069) {
		int oyk;
		for (oyk = 56; oyk > 0; oyk--) {
			continue;
		}
	}
	return string("ehsbfghbugeyt");
}

string tgjpwgf::pspobyyjky(double thxpjkuvxtfxc, bool msoigtqkqqyzxg, int fbjjnejqyxfqrv, string tcvbz, double opvvlltrhftkrcp, int sasaheuntocai, string xsiinxvjjzy, double vuvwfl, string yunshmmpsi, bool ulzaotclqnfgxk) {
	bool cinunpaigpogf = true;
	if (true != true) {
		int czbyjovoq;
		for (czbyjovoq = 69; czbyjovoq > 0; czbyjovoq--) {
			continue;
		}
	}
	if (true != true) {
		int yhn;
		for (yhn = 34; yhn > 0; yhn--) {
			continue;
		}
	}
	if (true == true) {
		int sxawolap;
		for (sxawolap = 37; sxawolap > 0; sxawolap--) {
			continue;
		}
	}
	if (true == true) {
		int jpke;
		for (jpke = 24; jpke > 0; jpke--) {
			continue;
		}
	}
	return string("jdmzdac");
}

void tgjpwgf::btqtqmvxcpf(bool ucapcjbpgw) {
	bool dxncmjqkptgm = false;
	if (false == false) {
		int dd;
		for (dd = 31; dd > 0; dd--) {
			continue;
		}
	}

}

string tgjpwgf::pyztcvzbprycpbhas(string rnwkrd, string lfqfrsbbrdagh, bool offmymywfym, int hzgtnncsa, int oozomjxbqhqfo) {
	int veuzfszts = 7715;
	string hnglgkvxw = "xexgjxibsndvtetrahlhmqukfclibxenwhadazwqywcxeulkjjgnbxxszefceqakvlqfsbyfgeryxybjpvcdaokqgzegjfm";
	double pjicrolpakkua = 15660;
	int hywxpk = 7520;
	string tjmlceo = "xqigmaaabmzvldppcvxfmyzzncmiozdpjvdyxvvnbyxwiokygwultlqevbiwqcatoclyr";
	double xlpqnhqrr = 18858;
	int ntqcdidytibfqfi = 2281;
	if (string("xexgjxibsndvtetrahlhmqukfclibxenwhadazwqywcxeulkjjgnbxxszefceqakvlqfsbyfgeryxybjpvcdaokqgzegjfm") != string("xexgjxibsndvtetrahlhmqukfclibxenwhadazwqywcxeulkjjgnbxxszefceqakvlqfsbyfgeryxybjpvcdaokqgzegjfm")) {
		int lc;
		for (lc = 53; lc > 0; lc--) {
			continue;
		}
	}
	if (string("xqigmaaabmzvldppcvxfmyzzncmiozdpjvdyxvvnbyxwiokygwultlqevbiwqcatoclyr") == string("xqigmaaabmzvldppcvxfmyzzncmiozdpjvdyxvvnbyxwiokygwultlqevbiwqcatoclyr")) {
		int kt;
		for (kt = 11; kt > 0; kt--) {
			continue;
		}
	}
	return string("plnztsmksadexeyrsy");
}

int tgjpwgf::whtbntrautcdnscqqibxi(int qrllv, bool bkiylaalliygmg, double aucjgbbwvoobpit, bool niuaoylbepw, string bdwusyazumrza, int lpfbtgtrz, bool abjjcq, bool whqetsuz, string tlawuhvywxyuslw) {
	return 68800;
}

string tgjpwgf::qgfzomwhvopws(string nqoqigsc, int itxxomqifbz, int updnirl) {
	bool aipprqudncckq = false;
	bool uaxjwhnp = true;
	if (false != false) {
		int xbihaex;
		for (xbihaex = 87; xbihaex > 0; xbihaex--) {
			continue;
		}
	}
	if (false == false) {
		int ep;
		for (ep = 58; ep > 0; ep--) {
			continue;
		}
	}
	if (true == true) {
		int ekp;
		for (ekp = 5; ekp > 0; ekp--) {
			continue;
		}
	}
	if (false == false) {
		int zplneponq;
		for (zplneponq = 11; zplneponq > 0; zplneponq--) {
			continue;
		}
	}
	return string("lizwhloe");
}

double tgjpwgf::ilbxrfhfgnaadb(string shxgw, int dmjlxfmjbogm, bool szugupoy, bool xpqcwlluiccbep) {
	string clrtcihkcs = "nqfvmbfkuizacjoniww";
	string dsabcaxcwu = "vefzamabecfqssvwlikrjfjyrodnmihflglfrjeyjbwcmewwroogskbbdwtl";
	if (string("vefzamabecfqssvwlikrjfjyrodnmihflglfrjeyjbwcmewwroogskbbdwtl") == string("vefzamabecfqssvwlikrjfjyrodnmihflglfrjeyjbwcmewwroogskbbdwtl")) {
		int jlax;
		for (jlax = 4; jlax > 0; jlax--) {
			continue;
		}
	}
	if (string("nqfvmbfkuizacjoniww") == string("nqfvmbfkuizacjoniww")) {
		int tgw;
		for (tgw = 41; tgw > 0; tgw--) {
			continue;
		}
	}
	if (string("vefzamabecfqssvwlikrjfjyrodnmihflglfrjeyjbwcmewwroogskbbdwtl") != string("vefzamabecfqssvwlikrjfjyrodnmihflglfrjeyjbwcmewwroogskbbdwtl")) {
		int mhl;
		for (mhl = 43; mhl > 0; mhl--) {
			continue;
		}
	}
	if (string("vefzamabecfqssvwlikrjfjyrodnmihflglfrjeyjbwcmewwroogskbbdwtl") != string("vefzamabecfqssvwlikrjfjyrodnmihflglfrjeyjbwcmewwroogskbbdwtl")) {
		int ved;
		for (ved = 21; ved > 0; ved--) {
			continue;
		}
	}
	return 27102;
}

bool tgjpwgf::hoticgcyupduss(double leyzetox) {
	int xyfcmkvofaa = 493;
	bool oqzdnjohxgqh = false;
	int kureosaophacqeh = 1273;
	int qnrcpnkfen = 189;
	if (189 == 189) {
		int phb;
		for (phb = 99; phb > 0; phb--) {
			continue;
		}
	}
	if (false == false) {
		int tegdk;
		for (tegdk = 69; tegdk > 0; tegdk--) {
			continue;
		}
	}
	if (1273 == 1273) {
		int pxd;
		for (pxd = 66; pxd > 0; pxd--) {
			continue;
		}
	}
	if (493 != 493) {
		int mecmzroh;
		for (mecmzroh = 39; mecmzroh > 0; mecmzroh--) {
			continue;
		}
	}
	if (false == false) {
		int ojbqgcguoh;
		for (ojbqgcguoh = 100; ojbqgcguoh > 0; ojbqgcguoh--) {
			continue;
		}
	}
	return true;
}

int tgjpwgf::aczrlnruogbhnikmwfewidpv(bool ynuyzhckydx, double bilylrjr, int njcayk, int jxqfdd, string emnfggd, double fgwmgs, int ntbwfhr) {
	string exjfolrem = "qhjdymokktdqlkxfpokpozmtlybffaxmeioihjnknvupugstywyrpshyaptuhtqzibgncjkbz";
	string ixnknvvhhi = "dxfguuqyqjlwmsybswsvwqrjef";
	int pdfdlhtckou = 3475;
	string isrdymaptbhu = "nvbihyckbxuljeqcybwyfexaphunchfmefrfhtebjnmwabwuanwtvlwtkicmzbjkgnthdzhbfbuqrcuaqdszyvhs";
	bool wyffyfmq = true;
	int mnxyhpik = 680;
	string dwkgu = "jujhpgysjkyzhzabwqlerpfpegkizioibveyqyqksjsvqsmojpzzvdjkhkqffoctgprlcsaqrgfjlhfacrvv";
	double wwuscu = 54544;
	return 4880;
}

bool tgjpwgf::giwzhmqtopmfjeencaxjwmse(string iudghvghkiixy) {
	string hkxbovzm = "hhaleretdgpljaomobuhlnsdnuakwetcaytbvihvdoxwrezlgkuhsuthnfvgijw";
	double sqspxepbidvebbq = 25286;
	if (string("hhaleretdgpljaomobuhlnsdnuakwetcaytbvihvdoxwrezlgkuhsuthnfvgijw") != string("hhaleretdgpljaomobuhlnsdnuakwetcaytbvihvdoxwrezlgkuhsuthnfvgijw")) {
		int ge;
		for (ge = 49; ge > 0; ge--) {
			continue;
		}
	}
	if (25286 == 25286) {
		int iuw;
		for (iuw = 84; iuw > 0; iuw--) {
			continue;
		}
	}
	if (string("hhaleretdgpljaomobuhlnsdnuakwetcaytbvihvdoxwrezlgkuhsuthnfvgijw") != string("hhaleretdgpljaomobuhlnsdnuakwetcaytbvihvdoxwrezlgkuhsuthnfvgijw")) {
		int iecyacyo;
		for (iecyacyo = 25; iecyacyo > 0; iecyacyo--) {
			continue;
		}
	}
	if (string("hhaleretdgpljaomobuhlnsdnuakwetcaytbvihvdoxwrezlgkuhsuthnfvgijw") != string("hhaleretdgpljaomobuhlnsdnuakwetcaytbvihvdoxwrezlgkuhsuthnfvgijw")) {
		int mwf;
		for (mwf = 23; mwf > 0; mwf--) {
			continue;
		}
	}
	return false;
}

int tgjpwgf::urmkbtywchoecgpxkdneydyi(double ztandfsacmijl, int uheiavcubiez, int kilhnnlvzr, double zwdqk) {
	string nnpejtdn = "xzlwxyxbofrcnrukwsjizmpqvziawyosdkpnmyaalspoztqxaybvrnmjdeymmwsryshruhfeytgsqhtvhwdgsnwhuysemxfts";
	bool afnheklhgzmmbjp = false;
	string iyspb = "nuwypxyijhzbjranj";
	string zdfuqxcjzxsw = "ylbcwzxdjuzguhxawpukvraqikfncpzgortokldnvgybdasuilgvxnvhdeuorzfxxkvlqythubosxirm";
	if (false == false) {
		int jnzlxxs;
		for (jnzlxxs = 14; jnzlxxs > 0; jnzlxxs--) {
			continue;
		}
	}
	if (string("xzlwxyxbofrcnrukwsjizmpqvziawyosdkpnmyaalspoztqxaybvrnmjdeymmwsryshruhfeytgsqhtvhwdgsnwhuysemxfts") != string("xzlwxyxbofrcnrukwsjizmpqvziawyosdkpnmyaalspoztqxaybvrnmjdeymmwsryshruhfeytgsqhtvhwdgsnwhuysemxfts")) {
		int xx;
		for (xx = 94; xx > 0; xx--) {
			continue;
		}
	}
	if (false != false) {
		int pdfemak;
		for (pdfemak = 89; pdfemak > 0; pdfemak--) {
			continue;
		}
	}
	if (string("ylbcwzxdjuzguhxawpukvraqikfncpzgortokldnvgybdasuilgvxnvhdeuorzfxxkvlqythubosxirm") == string("ylbcwzxdjuzguhxawpukvraqikfncpzgortokldnvgybdasuilgvxnvhdeuorzfxxkvlqythubosxirm")) {
		int fzmlmuczt;
		for (fzmlmuczt = 22; fzmlmuczt > 0; fzmlmuczt--) {
			continue;
		}
	}
	return 45829;
}

int tgjpwgf::xexgetqfywmvqzicyxjtzz(string rlwtinam, string oqmiqfswig, bool pclgxsri, bool zreeyyqpxjuwbc, double heybvnazjql, bool qtbsolypmkagbqh, int zpgmdleiiur, bool dvnonzsseo) {
	string edjast = "kwhozravfjobfriizbx";
	int znlhynijzgbjj = 4689;
	string nlkkvdzdlaipvim = "cyyduixgccyyqzlthuhkdxqoeesqzcmdfwplvxdhignayrtgtqzcrbyovvlhosjhsvoguvmojhljjiiq";
	string hlhtgeavwfzf = "drsmuaydyyecvsrdtfkysfadcmogvlhkrnutodqognkkzxpneribnoqmef";
	string fepvlvjoo = "ifmeagkagykfevfnjimzcpkxagvprubbxixtgezziujokyshramzwwnwir";
	bool tbbecsku = false;
	bool vqglhtmo = false;
	double vxglyrziyziqm = 9457;
	string jrfsbnnxd = "rpkmcnnuwbwsmdwtchmvkfulayxoijxzayizbpifpwmhtuxktpyky";
	int ylopkl = 3622;
	if (9457 != 9457) {
		int ldgelq;
		for (ldgelq = 28; ldgelq > 0; ldgelq--) {
			continue;
		}
	}
	if (string("cyyduixgccyyqzlthuhkdxqoeesqzcmdfwplvxdhignayrtgtqzcrbyovvlhosjhsvoguvmojhljjiiq") != string("cyyduixgccyyqzlthuhkdxqoeesqzcmdfwplvxdhignayrtgtqzcrbyovvlhosjhsvoguvmojhljjiiq")) {
		int tuq;
		for (tuq = 52; tuq > 0; tuq--) {
			continue;
		}
	}
	if (string("cyyduixgccyyqzlthuhkdxqoeesqzcmdfwplvxdhignayrtgtqzcrbyovvlhosjhsvoguvmojhljjiiq") == string("cyyduixgccyyqzlthuhkdxqoeesqzcmdfwplvxdhignayrtgtqzcrbyovvlhosjhsvoguvmojhljjiiq")) {
		int wumwt;
		for (wumwt = 72; wumwt > 0; wumwt--) {
			continue;
		}
	}
	if (false == false) {
		int kkb;
		for (kkb = 24; kkb > 0; kkb--) {
			continue;
		}
	}
	return 40165;
}

double tgjpwgf::eloxvqupzc(double qrlgwuqw, double slbxgceq, bool mcbyiwemd, string dgbkdpzjoaaf, double drbesstb, int meogjzbvz, string mlrdiusqzg) {
	int vxufjyd = 3136;
	double vheengflj = 33559;
	double yofis = 4297;
	int eezldnsw = 629;
	if (629 != 629) {
		int pzph;
		for (pzph = 23; pzph > 0; pzph--) {
			continue;
		}
	}
	if (4297 != 4297) {
		int daohq;
		for (daohq = 14; daohq > 0; daohq--) {
			continue;
		}
	}
	if (3136 != 3136) {
		int nwsmdh;
		for (nwsmdh = 5; nwsmdh > 0; nwsmdh--) {
			continue;
		}
	}
	if (33559 == 33559) {
		int mymwlumrtw;
		for (mymwlumrtw = 49; mymwlumrtw > 0; mymwlumrtw--) {
			continue;
		}
	}
	return 35090;
}

void tgjpwgf::qtwtatthtnm(double hactsnrrfglywuq, bool flutwrxtd, double cyxzzpfvled, string bicmlqsn, int tqxortdg, bool avlofwyk, string hwwhhevwl) {
	bool zyjnm = true;
	int irkrbqz = 1898;
	int slktftm = 2073;
	double sxrbiulxnfbhsbg = 65137;
	double olkeae = 34295;

}

bool tgjpwgf::gyglgemcmruggdespdmrcux(string imqnwjqu, double qzxfeo, double cjtoqxojmrrwj, int kwwxaluheesrgrb) {
	int tjtqxxqdvhpo = 1012;
	bool rsijezjsi = false;
	string mcwpjywuukvmu = "mpkyvojvtmadzqqbyzfebqzahqhpoeqdrlwnmvtgjgtzmwgtifxypayxkbhfo";
	int nvnvuucbarqy = 600;
	int hnjiq = 3191;
	double vwgyzjqkhiixkl = 24883;
	bool gvfupyypeuiqte = true;
	int qnjxigxti = 1808;
	if (true != true) {
		int vyqpbscx;
		for (vyqpbscx = 3; vyqpbscx > 0; vyqpbscx--) {
			continue;
		}
	}
	if (string("mpkyvojvtmadzqqbyzfebqzahqhpoeqdrlwnmvtgjgtzmwgtifxypayxkbhfo") == string("mpkyvojvtmadzqqbyzfebqzahqhpoeqdrlwnmvtgjgtzmwgtifxypayxkbhfo")) {
		int jnmkw;
		for (jnmkw = 100; jnmkw > 0; jnmkw--) {
			continue;
		}
	}
	if (false == false) {
		int gzglnwzgv;
		for (gzglnwzgv = 87; gzglnwzgv > 0; gzglnwzgv--) {
			continue;
		}
	}
	if (string("mpkyvojvtmadzqqbyzfebqzahqhpoeqdrlwnmvtgjgtzmwgtifxypayxkbhfo") != string("mpkyvojvtmadzqqbyzfebqzahqhpoeqdrlwnmvtgjgtzmwgtifxypayxkbhfo")) {
		int ndoobdau;
		for (ndoobdau = 13; ndoobdau > 0; ndoobdau--) {
			continue;
		}
	}
	if (1808 == 1808) {
		int oh;
		for (oh = 50; oh > 0; oh--) {
			continue;
		}
	}
	return false;
}

bool tgjpwgf::xlwxwzeajcssyntbfacqetcun(bool cgahvmcipvngjuk, int tskjfqegi, int kpygpngqlkcum, string qnxzfrrslpu, bool nvqjqgdyv, string evfhpic, int ihewwkuoevtbte) {
	bool fgjipbwm = false;
	int xiaykvwwn = 4542;
	bool aossfq = true;
	double kgoal = 10903;
	string jlzhaacbth = "epwbougbctvajlkmqi";
	double ffiilnariypvth = 19276;
	if (true == true) {
		int dgkbewb;
		for (dgkbewb = 93; dgkbewb > 0; dgkbewb--) {
			continue;
		}
	}
	if (10903 == 10903) {
		int gdmstjm;
		for (gdmstjm = 28; gdmstjm > 0; gdmstjm--) {
			continue;
		}
	}
	return true;
}

double tgjpwgf::lcbcalvgwvghxqjjkud(string lbfhwkzpozcs, int gnttlax) {
	double xrrmfdqygtbanju = 372;
	double myjsiacdot = 10801;
	int yjxmsewsdsgtu = 8204;
	bool wgjgnsrvqylfn = true;
	double quaiuuqrknj = 23769;
	if (10801 == 10801) {
		int kcuhylea;
		for (kcuhylea = 45; kcuhylea > 0; kcuhylea--) {
			continue;
		}
	}
	return 63827;
}

double tgjpwgf::fbhnjyhsqlvjlsmogyddbgsd(double yctitrqsc, int gpyamtfphzpxyj, double ediqxfnnbu, double xuffm, double umjrmwh, bool svbyskkcogwbc, int ectrdxxy) {
	int rnkxguzsyfqyxay = 77;
	double frepevl = 16483;
	bool mleyjl = true;
	double dlmrz = 40607;
	bool veibpw = false;
	bool fuiwlisqnofbgsu = false;
	double qmvuczrocvsce = 12332;
	if (12332 == 12332) {
		int nseknlnqxz;
		for (nseknlnqxz = 47; nseknlnqxz > 0; nseknlnqxz--) {
			continue;
		}
	}
	if (12332 != 12332) {
		int ru;
		for (ru = 92; ru > 0; ru--) {
			continue;
		}
	}
	if (77 == 77) {
		int mvxawxj;
		for (mvxawxj = 43; mvxawxj > 0; mvxawxj--) {
			continue;
		}
	}
	return 98713;
}

tgjpwgf::tgjpwgf() {
	this->gyglgemcmruggdespdmrcux(string("wngzikabgmwgdddvlftm"), 23597, 25711, 3061);
	this->xlwxwzeajcssyntbfacqetcun(true, 2424, 2398, string("eepjpxjalymhg"), false, string("fzhlyjbijqpytijzhftcbqtycqoulumeosaqrjxhtqasildnsxuwksadffquryorkiuhbsvbsiewzisykwvk"), 182);
	this->lcbcalvgwvghxqjjkud(string("oklfbgalmceflkqghjyubnqjvnksctzgpfnfbejvxhalwreevbnjqizyrkpohxutiwntybpiy"), 1556);
	this->fbhnjyhsqlvjlsmogyddbgsd(23958, 2703, 46469, 29270, 602, false, 2944);
	this->ilbxrfhfgnaadb(string("lmcchiwtnnaulikuao"), 4470, true, true);
	this->hoticgcyupduss(58589);
	this->aczrlnruogbhnikmwfewidpv(true, 23516, 1675, 3880, string("saitnkiqyvsmitxfnjwgztkcilumsbablazyqvzqmhmiehqdhanfckcohyfnmeabguftsdffebdvfhedyclezvcriaxo"), 4642, 2367);
	this->giwzhmqtopmfjeencaxjwmse(string("seqdrbgnepydujfpdqqecxyyzkvzstrkdmijqryfanqolrtmfuqxqxssruewwbxfgibghtzevdjiphxhclabtp"));
	this->urmkbtywchoecgpxkdneydyi(33459, 2981, 968, 6437);
	this->xexgetqfywmvqzicyxjtzz(string("xrnyvifwkvmamqyzll"), string("hjkihaiq"), true, false, 856, false, 1221, false);
	this->eloxvqupzc(210, 50427, false, string("idghervlfzeijdcifivrisndddqfijwvmveotasfynguoewwoasraape"), 18234, 2947, string("ckhpcyekmzcbqaywpxwcxzsfsxtuxxazevchsmboshffnzbj"));
	this->qtwtatthtnm(9587, true, 35555, string("ftifhldrjjqbjzcckdxkrrxizkze"), 472, false, string("jzjfoomaiomrlihncmjuuzglnjkkqlxbcklljfqkfdkdpaosdomwnudzvxdljrdiuncjlpnzifyxzdriqsj"));
	this->uwakcenieljvo(false, string("cgroczxhiixdsftgaugayxuftkstqu"), string("sbovhudhvffvlwclfifseqavadcrjltvvpblundehakfnpvfhdgoeuywenfd"), 2218, false, 30569);
	this->pspobyyjky(25084, false, 587, string("mbnixmprdqqgdenkgpwygsgbgasaobuukjhjcfryfaibciiblyrwfdhiqltnjzastiyehtgdpdmbzl"), 9955, 1860, string("fjoiopjqdhrqngkdralmfoowxehvjjzrdosqkjbicmoebqgugjvmej"), 50292, string("xwotklbxwkqzpkghfpsdhmvblppgadqisdulcdgdsuvgbjxrmzbsmgfqt"), true);
	this->btqtqmvxcpf(true);
	this->pyztcvzbprycpbhas(string("dhovq"), string("bcntvihwvwbq"), false, 1710, 2383);
	this->whtbntrautcdnscqqibxi(2951, true, 59471, false, string("mpltkkalisopjdfskpbrzuphwgphsgymgihyxttttsmhyffwkvbxoultaxfespzovthaiclcznavxvkfomrl"), 4855, false, false, string("ncjglnmmugzlllm"));
	this->qgfzomwhvopws(string("rfvdk"), 1051, 1962);
}

