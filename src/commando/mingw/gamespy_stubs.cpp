#include "gamespyadmin.h"
#include "GameSpy_QnR.h"
#include "gamespyauthmgr.h"
#include "GameSpyBanList.h"
#include "CDKeyAuth.h"
#include "wwstring.h"
#include "widestring.h"
#include "listnode.h"
#include "DlgWOLWait.h"

static WideStringClass s_player_nickname;

bool cGameSpyAdmin::DetectingBandwidth = false;
bool cGameSpyAdmin::IsUnderGamespyMenuing = false;
bool cGameSpyAdmin::IsLaunchFromGamespyRequested = false;
bool cGameSpyAdmin::IsLaunchedFromGamespy = false;
bool cGameSpyAdmin::IsServerGamespyListed = false;
ULONG cGameSpyAdmin::GameHostIp = 0;
USHORT cGameSpyAdmin::GameHostPort = 0;
WideStringClass cGameSpyAdmin::PasswordAttempt;

void cGameSpyAdmin::Think(void) {}
void cGameSpyAdmin::HandleNotification(DlgWOLWaitEvent &) {}
void cGameSpyAdmin::Join_Server(void) {}
void cGameSpyAdmin::Reset(void) { IsUnderGamespyMenuing = false; IsLaunchFromGamespyRequested = false; IsLaunchedFromGamespy = false; IsServerGamespyListed = false; GameHostIp = 0; GameHostPort = 0; }
void cGameSpyAdmin::Connect_To_Game_Server(void) {}
void cGameSpyAdmin::Set_Game_Host_Ip(ULONG ip) { GameHostIp = ip; }
void cGameSpyAdmin::Set_Game_Host_Port(USHORT port) { GameHostPort = port; }
bool cGameSpyAdmin::Is_Gamespy_Game(void) { return false; }
bool cGameSpyAdmin::Is_Nickname_Collision(WideStringClass &) { return false; }
void cGameSpyAdmin::Set_Player_Nickname(WideStringClass &nickname) { s_player_nickname = nickname; }

const char *CGameSpyQnR::gamename = "ccrenegade";
const char *CGameSpyQnR::bname = "Retail";
const int CGameSpyQnR::prodid = 10064;
const int CGameSpyQnR::cdkey_id = 577;
const char *CGameSpyQnR::default_heartbeat_list = "master.gamespy.com:27900";

CGameSpyQnR::CGameSpyQnR() : m_GSInit(FALSE), m_GSEnabled(FALSE), StartTime(0) { secret_key[0] = 0; query_reporting_rec = 0; }
CGameSpyQnR::~CGameSpyQnR() {}
void CGameSpyQnR::Init(void) {}
void CGameSpyQnR::LaunchArcade(void) {}
void CGameSpyQnR::TrackUsage(void) {}
void CGameSpyQnR::Shutdown(void) {}
BOOL CGameSpyQnR::Parse_HeartBeat_List(const char *) { return TRUE; }
void CGameSpyQnR::Think(void) {}
void CGameSpyQnR::DoGameStuff(void) {}
BOOL CGameSpyQnR::Append_InfoKey_Pair(char *, int, const char *, const char *) { return FALSE; }
BOOL CGameSpyQnR::Append_InfoKey_Pair(char *, int, const char *, const StringClass &) { return FALSE; }
BOOL CGameSpyQnR::Append_InfoKey_Pair(char *, int, const char *, const WideStringClass &) { return FALSE; }
void CGameSpyQnR::basic_callback(char *, int) {}
void CGameSpyQnR::info_callback(char *, int) {}
void CGameSpyQnR::rules_callback(char *, int) {}
void CGameSpyQnR::players_callback(char *, int) {}

CGameSpyQnR GameSpyQnR;

void cGameSpyAuthMgr::Think(void) {}
void cGameSpyAuthMgr::Initiate_Auth_Rejection(int) {}
LPCSTR cGameSpyAuthMgr::Describe_Auth_State(GAMESPY_AUTH_STATE_ENUM) { return ""; }
void cGameSpyAuthMgr::Evict_Player(int) {}

BanEntry::BanEntry(const char *, const char *, const char *, const char *, bool) {}
cGameSpyBanList::cGameSpyBanList() : BanList(NULL) {}
cGameSpyBanList::~cGameSpyBanList() {}
void cGameSpyBanList::Think(void) {}
void cGameSpyBanList::Ban_User(const char *, const char *, ULONG) {}
bool cGameSpyBanList::Is_User_Banned(const char *, const char *, ULONG) { return false; }
void cGameSpyBanList::LoadBans(void) {}
bool cGameSpyBanList::Final_Player_Kick(int) { return false; }
bool cGameSpyBanList::Begin_Player_Kick(int) { return false; }
void cGameSpyBanList::Strip_Escapes(char *) {}

cGameSpyBanList GameSpyBanList;

char *CCDKeyAuth::GenChallenge(int) { static char challenge[] = "12345678"; return challenge; }
void CCDKeyAuth::auth_callback(int, int, char *, void *) {}
void CCDKeyAuth::DisconnectUser(int) {}
void CCDKeyAuth::AuthenticateUser(int, ULONG, char *, char *) {}
void CCDKeyAuth::AuthSerial(const char *, StringClass &resp) { resp = "stub"; }
void CCDKeyAuth::GetSerialNum(StringClass &serial) { serial = "0000000-0000000"; }
