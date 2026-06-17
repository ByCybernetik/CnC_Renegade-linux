/* Auto-generated for MinGW scripts.dll - do not edit by hand. */
#include "scriptcommands_proxy.h"

#include <stdio.h>
#include <stdarg.h>

ScriptCommandsProxy g_ScriptCommandsProxy;

void ScriptCommandsProxy::Action_Reset(GameObject * obj, float priority)
{
	if (!m_p) return;
	m_p->Action_Reset(obj, priority);
}

void ScriptCommandsProxy::Action_Goto(GameObject * obj, const ActionParamsStruct & params)
{
	if (!m_p) return;
	m_p->Action_Goto(obj, params);
}

void ScriptCommandsProxy::Action_Attack(GameObject * obj, const ActionParamsStruct & params)
{
	if (!m_p) return;
	m_p->Action_Attack(obj, params);
}

void ScriptCommandsProxy::Action_Play_Animation(GameObject * obj, const ActionParamsStruct & params)
{
	if (!m_p) return;
	m_p->Action_Play_Animation(obj, params);
}

void ScriptCommandsProxy::Action_Enter_Exit(GameObject * obj, const ActionParamsStruct & params)
{
	if (!m_p) return;
	m_p->Action_Enter_Exit(obj, params);
}

void ScriptCommandsProxy::Action_Face_Location(GameObject * obj, const ActionParamsStruct & params)
{
	if (!m_p) return;
	m_p->Action_Face_Location(obj, params);
}

void ScriptCommandsProxy::Action_Dock(GameObject * obj, const ActionParamsStruct & params)
{
	if (!m_p) return;
	m_p->Action_Dock(obj, params);
}

void ScriptCommandsProxy::Action_Follow_Input(GameObject * obj, const ActionParamsStruct & params)
{
	if (!m_p) return;
	m_p->Action_Follow_Input(obj, params);
}

void ScriptCommandsProxy::Modify_Action(GameObject * obj, int action_id, const ActionParamsStruct & params, bool modify_move, bool modify_attack)
{
	if (!m_p) return;
	m_p->Modify_Action(obj, action_id, params, modify_move, modify_attack);
}

int ScriptCommandsProxy::Get_Action_ID(GameObject * obj)
{
	if (!m_p) return 0;
	return m_p->Get_Action_ID(obj);
}

bool ScriptCommandsProxy::Get_Action_Params(GameObject * obj, ActionParamsStruct & params)
{
	if (!m_p) return false;
	return m_p->Get_Action_Params(obj, params);
}

bool ScriptCommandsProxy::Is_Performing_Pathfind_Action(GameObject * obj)
{
	if (!m_p) return false;
	return m_p->Is_Performing_Pathfind_Action(obj);
}

void ScriptCommandsProxy::Set_Position(GameObject * obj, const Vector3 & position)
{
	if (!m_p) return;
	m_p->Set_Position(obj, position);
}

Vector3 ScriptCommandsProxy::Get_Position(GameObject * obj)
{
	if (!m_p) return 0;
	return m_p->Get_Position(obj);
}

Vector3 ScriptCommandsProxy::Get_Bone_Position(GameObject * obj, const char * bone_name)
{
	if (!m_p) return 0;
	return m_p->Get_Bone_Position(obj, bone_name);
}

float ScriptCommandsProxy::Get_Facing(GameObject * obj)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Facing(obj);
}

void ScriptCommandsProxy::Set_Facing(GameObject * obj, float degrees)
{
	if (!m_p) return;
	m_p->Set_Facing(obj, degrees);
}

void ScriptCommandsProxy::Disable_All_Collisions(GameObject * obj)
{
	if (!m_p) return;
	m_p->Disable_All_Collisions(obj);
}

void ScriptCommandsProxy::Disable_Physical_Collisions(GameObject * obj)
{
	if (!m_p) return;
	m_p->Disable_Physical_Collisions(obj);
}

void ScriptCommandsProxy::Enable_Collisions(GameObject * obj)
{
	if (!m_p) return;
	m_p->Enable_Collisions(obj);
}

void ScriptCommandsProxy::Destroy_Object(GameObject * obj)
{
	if (!m_p) return;
	m_p->Destroy_Object(obj);
}

GameObject * ScriptCommandsProxy::Find_Object(int obj_id)
{
	if (!m_p) return NULL;
	return m_p->Find_Object(obj_id);
}

GameObject * ScriptCommandsProxy::Create_Object(const char * type_name, const Vector3 & position)
{
	if (!m_p) return NULL;
	return m_p->Create_Object(type_name, position);
}

GameObject * ScriptCommandsProxy::Create_Object_At_Bone(GameObject * host_obj, const char * new_obj_type_name, const char * bone_name)
{
	if (!m_p) return NULL;
	return m_p->Create_Object_At_Bone(host_obj, new_obj_type_name, bone_name);
}

int ScriptCommandsProxy::Get_ID(GameObject * obj)
{
	if (!m_p) return 0;
	return m_p->Get_ID(obj);
}

int ScriptCommandsProxy::Get_Preset_ID(GameObject * obj)
{
	if (!m_p) return 0;
	return m_p->Get_Preset_ID(obj);
}

const char * ScriptCommandsProxy::Get_Preset_Name(GameObject * obj)
{
	if (!m_p) return NULL;
	return m_p->Get_Preset_Name(obj);
}

void ScriptCommandsProxy::Attach_Script(GameObject* object, const char* scriptName, const char* scriptParams)
{
	if (!m_p) return;
	m_p->Attach_Script(object, scriptName, scriptParams);
}

void ScriptCommandsProxy::Add_To_Dirty_Cull_List(GameObject* object)
{
	if (!m_p) return;
	m_p->Add_To_Dirty_Cull_List(object);
}

void ScriptCommandsProxy::Start_Timer(GameObject * obj, ScriptClass * script, float duration, int timer_id)
{
	if (!m_p) return;
	m_p->Start_Timer(obj, script, duration, timer_id);
}

void ScriptCommandsProxy::Trigger_Weapon(GameObject * obj, bool trigger, const Vector3 & target, bool primary)
{
	if (!m_p) return;
	m_p->Trigger_Weapon(obj, trigger, target, primary);
}

void ScriptCommandsProxy::Select_Weapon(GameObject * obj, const char * weapon_name)
{
	if (!m_p) return;
	m_p->Select_Weapon(obj, weapon_name);
}

void ScriptCommandsProxy::Send_Custom_Event(GameObject * from, GameObject * to, int type, intptr_t param, float delay)
{
	if (!m_p) return;
	m_p->Send_Custom_Event(from, to, type, param, delay);
}

void ScriptCommandsProxy::Send_Damaged_Event(GameObject * object, GameObject * damager)
{
	if (!m_p) return;
	m_p->Send_Damaged_Event(object, damager);
}

float ScriptCommandsProxy::Get_Random(float min, float max)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Random(min, max);
}

int ScriptCommandsProxy::Get_Random_Int(int min, int max)
{
	if (!m_p) return 0;
	return m_p->Get_Random_Int(min, max);
}

GameObject * ScriptCommandsProxy::Find_Random_Simple_Object(const char *preset_name)
{
	if (!m_p) return NULL;
	return m_p->Find_Random_Simple_Object(preset_name);
}

void ScriptCommandsProxy::Set_Model(GameObject * obj, const char * model_name)
{
	if (!m_p) return;
	m_p->Set_Model(obj, model_name);
}

void ScriptCommandsProxy::Set_Animation(GameObject * obj, const char * anim_name, bool looping, const char * sub_obj_name, float start_frame, float end_frame, bool is_blended)
{
	if (!m_p) return;
	m_p->Set_Animation(obj, anim_name, looping, sub_obj_name, start_frame, end_frame, is_blended);
}

void ScriptCommandsProxy::Set_Animation_Frame(GameObject * obj, const char * anim_name, int frame)
{
	if (!m_p) return;
	m_p->Set_Animation_Frame(obj, anim_name, frame);
}

int ScriptCommandsProxy::Create_Sound(const char * sound_preset_name, const Vector3 & position, GameObject * creator)
{
	if (!m_p) return 0;
	return m_p->Create_Sound(sound_preset_name, position, creator);
}

int ScriptCommandsProxy::Create_2D_Sound(const char * sound_preset_name)
{
	if (!m_p) return 0;
	return m_p->Create_2D_Sound(sound_preset_name);
}

int ScriptCommandsProxy::Create_2D_WAV_Sound(const char * wav_filename)
{
	if (!m_p) return 0;
	return m_p->Create_2D_WAV_Sound(wav_filename);
}

int ScriptCommandsProxy::Create_3D_WAV_Sound_At_Bone(const char * wav_filename, GameObject * obj, const char * bone_name)
{
	if (!m_p) return 0;
	return m_p->Create_3D_WAV_Sound_At_Bone(wav_filename, obj, bone_name);
}

int ScriptCommandsProxy::Create_3D_Sound_At_Bone(const char * sound_preset_name, GameObject * obj, const char * bone_name)
{
	if (!m_p) return 0;
	return m_p->Create_3D_Sound_At_Bone(sound_preset_name, obj, bone_name);
}

int ScriptCommandsProxy::Create_Logical_Sound(GameObject * creator, int type, const Vector3 & position, float radius)
{
	if (!m_p) return 0;
	return m_p->Create_Logical_Sound(creator, type, position, radius);
}

void ScriptCommandsProxy::Start_Sound(int sound_id)
{
	if (!m_p) return;
	m_p->Start_Sound(sound_id);
}

void ScriptCommandsProxy::Stop_Sound(int sound_id, bool destroy_sound)
{
	if (!m_p) return;
	m_p->Stop_Sound(sound_id, destroy_sound);
}

void ScriptCommandsProxy::Monitor_Sound(GameObject * game_obj, int sound_id)
{
	if (!m_p) return;
	m_p->Monitor_Sound(game_obj, sound_id);
}

void ScriptCommandsProxy::Set_Background_Music(const char * wav_filename)
{
	if (!m_p) return;
	m_p->Set_Background_Music(wav_filename);
}

void ScriptCommandsProxy::Fade_Background_Music(const char * wav_filename, int fade_out_time, int fade_in_time)
{
	if (!m_p) return;
	m_p->Fade_Background_Music(wav_filename, fade_out_time, fade_in_time);
}

void ScriptCommandsProxy::Stop_Background_Music(void)
{
	if (!m_p) return;
	m_p->Stop_Background_Music();
}

float ScriptCommandsProxy::Get_Health(GameObject * obj)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Health(obj);
}

float ScriptCommandsProxy::Get_Max_Health(GameObject * obj)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Max_Health(obj);
}

void ScriptCommandsProxy::Set_Health(GameObject * obj, float health)
{
	if (!m_p) return;
	m_p->Set_Health(obj, health);
}

float ScriptCommandsProxy::Get_Shield_Strength(GameObject * obj)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Shield_Strength(obj);
}

float ScriptCommandsProxy::Get_Max_Shield_Strength(GameObject * obj)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Max_Shield_Strength(obj);
}

void ScriptCommandsProxy::Set_Shield_Strength(GameObject * obj, float strength)
{
	if (!m_p) return;
	m_p->Set_Shield_Strength(obj, strength);
}

void ScriptCommandsProxy::Set_Shield_Type(GameObject * obj, const char * name)
{
	if (!m_p) return;
	m_p->Set_Shield_Type(obj, name);
}

int ScriptCommandsProxy::Get_Player_Type(GameObject * obj)
{
	if (!m_p) return 0;
	return m_p->Get_Player_Type(obj);
}

void ScriptCommandsProxy::Set_Player_Type(GameObject * obj, int type)
{
	if (!m_p) return;
	m_p->Set_Player_Type(obj, type);
}

float ScriptCommandsProxy::Get_Distance(const Vector3 & p1, const Vector3 & p2)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Distance(p1, p2);
}

void ScriptCommandsProxy::Set_Camera_Host(GameObject * obj)
{
	if (!m_p) return;
	m_p->Set_Camera_Host(obj);
}

void ScriptCommandsProxy::Force_Camera_Look(const Vector3 & target)
{
	if (!m_p) return;
	m_p->Force_Camera_Look(target);
}

GameObject * ScriptCommandsProxy::Get_The_Star(void)
{
	if (!m_p) return NULL;
	return m_p->Get_The_Star();
}

GameObject * ScriptCommandsProxy::Get_A_Star(const Vector3 & pos)
{
	if (!m_p) return NULL;
	return m_p->Get_A_Star(pos);
}

GameObject * ScriptCommandsProxy::Find_Closest_Soldier(const Vector3 & pos, float min_dist, float max_dist, bool only_human)
{
	if (!m_p) return NULL;
	return m_p->Find_Closest_Soldier(pos, min_dist, max_dist, only_human);
}

bool ScriptCommandsProxy::Is_A_Star(GameObject * obj)
{
	if (!m_p) return false;
	return m_p->Is_A_Star(obj);
}

void ScriptCommandsProxy::Control_Enable(GameObject * obj, bool enable)
{
	if (!m_p) return;
	m_p->Control_Enable(obj, enable);
}

const char * ScriptCommandsProxy::Get_Damage_Bone_Name(void)
{
	if (!m_p) return NULL;
	return m_p->Get_Damage_Bone_Name();
}

bool ScriptCommandsProxy::Get_Damage_Bone_Direction(void)
{
	if (!m_p) return false;
	return m_p->Get_Damage_Bone_Direction();
}

bool ScriptCommandsProxy::Is_Object_Visible(GameObject * looker, GameObject * obj)
{
	if (!m_p) return false;
	return m_p->Is_Object_Visible(looker, obj);
}

void ScriptCommandsProxy::Enable_Enemy_Seen(GameObject * obj, bool enable)
{
	if (!m_p) return;
	m_p->Enable_Enemy_Seen(obj, enable);
}

void ScriptCommandsProxy::Set_Display_Color(unsigned char red, unsigned char green, unsigned char blue)
{
	if (!m_p) return;
	m_p->Set_Display_Color(red, green, blue);
}

void ScriptCommandsProxy::Display_Text(int string_id)
{
	if (!m_p) return;
	m_p->Display_Text(string_id);
}

void ScriptCommandsProxy::Display_Float(float value, const char * format)
{
	if (!m_p) return;
	m_p->Display_Float(value, format);
}

void ScriptCommandsProxy::Display_Int(int value, const char * format)
{
	if (!m_p) return;
	m_p->Display_Int(value, format);
}

void ScriptCommandsProxy::Save_Data(ScriptSaver & saver, int id, int size, void * data)
{
	if (!m_p) return;
	m_p->Save_Data(saver, id, size, data);
}

void ScriptCommandsProxy::Save_Pointer(ScriptSaver & saver, int id, void * pointer)
{
	if (!m_p) return;
	m_p->Save_Pointer(saver, id, pointer);
}

bool ScriptCommandsProxy::Load_Begin(ScriptLoader & loader, int * id)
{
	if (!m_p) return false;
	return m_p->Load_Begin(loader, id);
}

void ScriptCommandsProxy::Load_Data(ScriptLoader & loader, int size, void * data)
{
	if (!m_p) return;
	m_p->Load_Data(loader, size, data);
}

void ScriptCommandsProxy::Load_Pointer(ScriptLoader & loader, void ** pointer)
{
	if (!m_p) return;
	m_p->Load_Pointer(loader, pointer);
}

void ScriptCommandsProxy::Load_End(ScriptLoader & loader)
{
	if (!m_p) return;
	m_p->Load_End(loader);
}

void ScriptCommandsProxy::Begin_Chunk(ScriptSaver& saver, unsigned int chunkID)
{
	if (!m_p) return;
	m_p->Begin_Chunk(saver, chunkID);
}

void ScriptCommandsProxy::End_Chunk(ScriptSaver& saver)
{
	if (!m_p) return;
	m_p->End_Chunk(saver);
}

bool ScriptCommandsProxy::Open_Chunk(ScriptLoader& loader, unsigned int* chunkID)
{
	if (!m_p) return false;
	return m_p->Open_Chunk(loader, chunkID);
}

void ScriptCommandsProxy::Close_Chunk(ScriptLoader& loader)
{
	if (!m_p) return;
	m_p->Close_Chunk(loader);
}

void ScriptCommandsProxy::Clear_Radar_Markers(void)
{
	if (!m_p) return;
	m_p->Clear_Radar_Markers();
}

void ScriptCommandsProxy::Clear_Radar_Marker(int id)
{
	if (!m_p) return;
	m_p->Clear_Radar_Marker(id);
}

void ScriptCommandsProxy::Add_Radar_Marker(int id, const Vector3& position, int shape_type, int color_type)
{
	if (!m_p) return;
	m_p->Add_Radar_Marker(id, position, shape_type, color_type);
}

void ScriptCommandsProxy::Set_Obj_Radar_Blip_Shape(GameObject * obj, int shape_type)
{
	if (!m_p) return;
	m_p->Set_Obj_Radar_Blip_Shape(obj, shape_type);
}

void ScriptCommandsProxy::Set_Obj_Radar_Blip_Color(GameObject * obj, int color_type)
{
	if (!m_p) return;
	m_p->Set_Obj_Radar_Blip_Color(obj, color_type);
}

void ScriptCommandsProxy::Enable_Radar(bool enable)
{
	if (!m_p) return;
	m_p->Enable_Radar(enable);
}

void ScriptCommandsProxy::Clear_Map_Cell(int cell_x, int cell_y)
{
	if (!m_p) return;
	m_p->Clear_Map_Cell(cell_x, cell_y);
}

void ScriptCommandsProxy::Clear_Map_Cell_By_Pos(const Vector3 &world_space_pos)
{
	if (!m_p) return;
	m_p->Clear_Map_Cell_By_Pos(world_space_pos);
}

void ScriptCommandsProxy::Clear_Map_Cell_By_Pixel_Pos(int pixel_pos_x, int pixel_pos_y)
{
	if (!m_p) return;
	m_p->Clear_Map_Cell_By_Pixel_Pos(pixel_pos_x, pixel_pos_y);
}

void ScriptCommandsProxy::Clear_Map_Region_By_Pos(const Vector3 &world_space_pos, int pixel_radius)
{
	if (!m_p) return;
	m_p->Clear_Map_Region_By_Pos(world_space_pos, pixel_radius);
}

void ScriptCommandsProxy::Reveal_Map(void)
{
	if (!m_p) return;
	m_p->Reveal_Map();
}

void ScriptCommandsProxy::Shroud_Map(void)
{
	if (!m_p) return;
	m_p->Shroud_Map();
}

void ScriptCommandsProxy::Show_Player_Map_Marker(bool onoff)
{
	if (!m_p) return;
	m_p->Show_Player_Map_Marker(onoff);
}

float ScriptCommandsProxy::Get_Safe_Flight_Height(float x_pos, float y_pos)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Safe_Flight_Height(x_pos, y_pos);
}

void ScriptCommandsProxy::Create_Explosion(const char * explosion_def_name, const Vector3 & pos, GameObject * creator)
{
	if (!m_p) return;
	m_p->Create_Explosion(explosion_def_name, pos, creator);
}

void ScriptCommandsProxy::Create_Explosion_At_Bone(const char * explosion_def_name, GameObject * object, const char * bone_name, GameObject * creator)
{
	if (!m_p) return;
	m_p->Create_Explosion_At_Bone(explosion_def_name, object, bone_name, creator);
}

void ScriptCommandsProxy::Enable_HUD(bool enable)
{
	if (!m_p) return;
	m_p->Enable_HUD(enable);
}

void ScriptCommandsProxy::Mission_Complete(bool success)
{
	if (!m_p) return;
	m_p->Mission_Complete(success);
}

void ScriptCommandsProxy::Give_PowerUp(GameObject * obj, const char * preset_name, bool display_on_hud)
{
	if (!m_p) return;
	m_p->Give_PowerUp(obj, preset_name, display_on_hud);
}

void ScriptCommandsProxy::Innate_Disable(GameObject* object)
{
	if (!m_p) return;
	m_p->Innate_Disable(object);
}

void ScriptCommandsProxy::Innate_Enable(GameObject* object)
{
	if (!m_p) return;
	m_p->Innate_Enable(object);
}

bool ScriptCommandsProxy::Innate_Soldier_Enable_Enemy_Seen(GameObject * obj, bool state)
{
	if (!m_p) return false;
	return m_p->Innate_Soldier_Enable_Enemy_Seen(obj, state);
}

bool ScriptCommandsProxy::Innate_Soldier_Enable_Gunshot_Heard(GameObject * obj, bool state)
{
	if (!m_p) return false;
	return m_p->Innate_Soldier_Enable_Gunshot_Heard(obj, state);
}

bool ScriptCommandsProxy::Innate_Soldier_Enable_Footsteps_Heard(GameObject * obj, bool state)
{
	if (!m_p) return false;
	return m_p->Innate_Soldier_Enable_Footsteps_Heard(obj, state);
}

bool ScriptCommandsProxy::Innate_Soldier_Enable_Bullet_Heard(GameObject * obj, bool state)
{
	if (!m_p) return false;
	return m_p->Innate_Soldier_Enable_Bullet_Heard(obj, state);
}

bool ScriptCommandsProxy::Innate_Soldier_Enable_Actions(GameObject * obj, bool state)
{
	if (!m_p) return false;
	return m_p->Innate_Soldier_Enable_Actions(obj, state);
}

void ScriptCommandsProxy::Set_Innate_Soldier_Home_Location(GameObject * obj, const Vector3& home_pos, float home_radius)
{
	if (!m_p) return;
	m_p->Set_Innate_Soldier_Home_Location(obj, home_pos, home_radius);
}

void ScriptCommandsProxy::Set_Innate_Aggressiveness(GameObject * obj, float aggressiveness)
{
	if (!m_p) return;
	m_p->Set_Innate_Aggressiveness(obj, aggressiveness);
}

void ScriptCommandsProxy::Set_Innate_Take_Cover_Probability(GameObject * obj, float probability)
{
	if (!m_p) return;
	m_p->Set_Innate_Take_Cover_Probability(obj, probability);
}

void ScriptCommandsProxy::Set_Innate_Is_Stationary(GameObject * obj, bool stationary)
{
	if (!m_p) return;
	m_p->Set_Innate_Is_Stationary(obj, stationary);
}

void ScriptCommandsProxy::Innate_Force_State_Bullet_Heard(GameObject * obj, const Vector3 & pos)
{
	if (!m_p) return;
	m_p->Innate_Force_State_Bullet_Heard(obj, pos);
}

void ScriptCommandsProxy::Innate_Force_State_Footsteps_Heard(GameObject * obj, const Vector3 & pos)
{
	if (!m_p) return;
	m_p->Innate_Force_State_Footsteps_Heard(obj, pos);
}

void ScriptCommandsProxy::Innate_Force_State_Gunshots_Heard(GameObject * obj, const Vector3 & pos)
{
	if (!m_p) return;
	m_p->Innate_Force_State_Gunshots_Heard(obj, pos);
}

void ScriptCommandsProxy::Innate_Force_State_Enemy_Seen(GameObject * obj, GameObject * enemy)
{
	if (!m_p) return;
	m_p->Innate_Force_State_Enemy_Seen(obj, enemy);
}

void ScriptCommandsProxy::Static_Anim_Phys_Goto_Frame(int obj_id, float frame, const char * anim_name)
{
	if (!m_p) return;
	m_p->Static_Anim_Phys_Goto_Frame(obj_id, frame, anim_name);
}

void ScriptCommandsProxy::Static_Anim_Phys_Goto_Last_Frame(int obj_id, const char * anim_name)
{
	if (!m_p) return;
	m_p->Static_Anim_Phys_Goto_Last_Frame(obj_id, anim_name);
}

unsigned int ScriptCommandsProxy::Get_Sync_Time(void)
{
	if (!m_p) return 0;
	return m_p->Get_Sync_Time();
}

void ScriptCommandsProxy::Add_Objective(int id, int type, int status, int short_description_id, char * description_sound_filename, int long_description_id)
{
	if (!m_p) return;
	m_p->Add_Objective(id, type, status, short_description_id, description_sound_filename, long_description_id);
}

void ScriptCommandsProxy::Remove_Objective(int id)
{
	if (!m_p) return;
	m_p->Remove_Objective(id);
}

void ScriptCommandsProxy::Set_Objective_Status(int id, int status)
{
	if (!m_p) return;
	m_p->Set_Objective_Status(id, status);
}

void ScriptCommandsProxy::Change_Objective_Type(int id, int type)
{
	if (!m_p) return;
	m_p->Change_Objective_Type(id, type);
}

void ScriptCommandsProxy::Set_Objective_Radar_Blip(int id, const Vector3 & position)
{
	if (!m_p) return;
	m_p->Set_Objective_Radar_Blip(id, position);
}

void ScriptCommandsProxy::Set_Objective_Radar_Blip_Object(int id, ScriptableGameObj * unit)
{
	if (!m_p) return;
	m_p->Set_Objective_Radar_Blip_Object(id, unit);
}

void ScriptCommandsProxy::Set_Objective_HUD_Info(int id, float priority, const char * texture_name, int message_id)
{
	if (!m_p) return;
	m_p->Set_Objective_HUD_Info(id, priority, texture_name, message_id);
}

void ScriptCommandsProxy::Set_Objective_HUD_Info_Position(int id, float priority, const char * texture_name, int message_id, const Vector3 & position)
{
	if (!m_p) return;
	m_p->Set_Objective_HUD_Info_Position(id, priority, texture_name, message_id, position);
}

void ScriptCommandsProxy::Shake_Camera(const Vector3 & pos, float radius, float intensity, float duration)
{
	if (!m_p) return;
	m_p->Shake_Camera(pos, radius, intensity, duration);
}

void ScriptCommandsProxy::Enable_Spawner(int id, bool enable)
{
	if (!m_p) return;
	m_p->Enable_Spawner(id, enable);
}

GameObject * ScriptCommandsProxy::Trigger_Spawner(int id)
{
	if (!m_p) return NULL;
	return m_p->Trigger_Spawner(id);
}

void ScriptCommandsProxy::Enable_Engine(GameObject* object, bool onoff)
{
	if (!m_p) return;
	m_p->Enable_Engine(object, onoff);
}

int ScriptCommandsProxy::Get_Difficulty_Level(void)
{
	if (!m_p) return 0;
	return m_p->Get_Difficulty_Level();
}

void ScriptCommandsProxy::Grant_Key(GameObject* object, int key, bool grant)
{
	if (!m_p) return;
	m_p->Grant_Key(object, key, grant);
}

bool ScriptCommandsProxy::Has_Key(GameObject* object, int key)
{
	if (!m_p) return false;
	return m_p->Has_Key(object, key);
}

void ScriptCommandsProxy::Enable_Hibernation(GameObject * object, bool enable)
{
	if (!m_p) return;
	m_p->Enable_Hibernation(object, enable);
}

void ScriptCommandsProxy::Attach_To_Object_Bone(GameObject * object, GameObject * host_object, const char * bone_name)
{
	if (!m_p) return;
	m_p->Attach_To_Object_Bone(object, host_object, bone_name);
}

int ScriptCommandsProxy::Create_Conversation(const char *conversation_name, int priority, float max_dist, bool is_interruptable)
{
	if (!m_p) return 0;
	return m_p->Create_Conversation(conversation_name, priority, max_dist, is_interruptable);
}

void ScriptCommandsProxy::Join_Conversation(GameObject * object, int active_conversation_id, bool allow_move, bool allow_head_turn, bool allow_face)
{
	if (!m_p) return;
	m_p->Join_Conversation(object, active_conversation_id, allow_move, allow_head_turn, allow_face);
}

void ScriptCommandsProxy::Join_Conversation_Facing(GameObject * object, int active_conversation_id, int obj_id_to_face)
{
	if (!m_p) return;
	m_p->Join_Conversation_Facing(object, active_conversation_id, obj_id_to_face);
}

void ScriptCommandsProxy::Start_Conversation(int active_conversation_id, int action_id)
{
	if (!m_p) return;
	m_p->Start_Conversation(active_conversation_id, action_id);
}

void ScriptCommandsProxy::Monitor_Conversation(GameObject * object, int active_conversation_id)
{
	if (!m_p) return;
	m_p->Monitor_Conversation(object, active_conversation_id);
}

void ScriptCommandsProxy::Start_Random_Conversation(GameObject * object)
{
	if (!m_p) return;
	m_p->Start_Random_Conversation(object);
}

void ScriptCommandsProxy::Stop_Conversation(int active_conversation_id)
{
	if (!m_p) return;
	m_p->Stop_Conversation(active_conversation_id);
}

void ScriptCommandsProxy::Stop_All_Conversations(void)
{
	if (!m_p) return;
	m_p->Stop_All_Conversations();
}

void ScriptCommandsProxy::Lock_Soldier_Facing(GameObject * object, GameObject * object_to_face, bool turn_body)
{
	if (!m_p) return;
	m_p->Lock_Soldier_Facing(object, object_to_face, turn_body);
}

void ScriptCommandsProxy::Unlock_Soldier_Facing(GameObject * object)
{
	if (!m_p) return;
	m_p->Unlock_Soldier_Facing(object);
}

void ScriptCommandsProxy::Apply_Damage(GameObject * object, float amount, const char * warhead_name, GameObject * damager)
{
	if (!m_p) return;
	m_p->Apply_Damage(object, amount, warhead_name, damager);
}

void ScriptCommandsProxy::Set_Loiters_Allowed(GameObject * object, bool allowed)
{
	if (!m_p) return;
	m_p->Set_Loiters_Allowed(object, allowed);
}

void ScriptCommandsProxy::Set_Is_Visible(GameObject * object, bool visible)
{
	if (!m_p) return;
	m_p->Set_Is_Visible(object, visible);
}

void ScriptCommandsProxy::Set_Is_Rendered(GameObject * object, bool rendered)
{
	if (!m_p) return;
	m_p->Set_Is_Rendered(object, rendered);
}

float ScriptCommandsProxy::Get_Points(GameObject * object)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Points(object);
}

void ScriptCommandsProxy::Give_Points(GameObject * object, float points, bool entire_team)
{
	if (!m_p) return;
	m_p->Give_Points(object, points, entire_team);
}

float ScriptCommandsProxy::Get_Money(GameObject * object)
{
	if (!m_p) return 0.0f;
	return m_p->Get_Money(object);
}

void ScriptCommandsProxy::Give_Money(GameObject * object, float money, bool entire_team)
{
	if (!m_p) return;
	m_p->Give_Money(object, money, entire_team);
}

bool ScriptCommandsProxy::Get_Building_Power(GameObject * object)
{
	if (!m_p) return false;
	return m_p->Get_Building_Power(object);
}

void ScriptCommandsProxy::Set_Building_Power(GameObject * object, bool onoff)
{
	if (!m_p) return;
	m_p->Set_Building_Power(object, onoff);
}

void ScriptCommandsProxy::Play_Building_Announcement(GameObject * object, int text_id)
{
	if (!m_p) return;
	m_p->Play_Building_Announcement(object, text_id);
}

GameObject * ScriptCommandsProxy::Find_Nearest_Building_To_Pos(const Vector3 & position, const char * mesh_prefix)
{
	if (!m_p) return NULL;
	return m_p->Find_Nearest_Building_To_Pos(position, mesh_prefix);
}

GameObject * ScriptCommandsProxy::Find_Nearest_Building(GameObject * object, const char * mesh_prefix)
{
	if (!m_p) return NULL;
	return m_p->Find_Nearest_Building(object, mesh_prefix);
}

int ScriptCommandsProxy::Team_Members_In_Zone(GameObject * object, int player_type)
{
	if (!m_p) return 0;
	return m_p->Team_Members_In_Zone(object, player_type);
}

void ScriptCommandsProxy::Set_Clouds(float cloudcover, float cloudgloominess, float ramptime)
{
	if (!m_p) return;
	m_p->Set_Clouds(cloudcover, cloudgloominess, ramptime);
}

void ScriptCommandsProxy::Set_Lightning(float intensity, float startdistance, float enddistance, float heading, float distribution, float ramptime)
{
	if (!m_p) return;
	m_p->Set_Lightning(intensity, startdistance, enddistance, heading, distribution, ramptime);
}

void ScriptCommandsProxy::Set_War_Blitz(float intensity, float startdistance, float enddistance, float heading, float distribution, float ramptime)
{
	if (!m_p) return;
	m_p->Set_War_Blitz(intensity, startdistance, enddistance, heading, distribution, ramptime);
}

void ScriptCommandsProxy::Set_Wind(float heading, float speed, float variability, float ramptime)
{
	if (!m_p) return;
	m_p->Set_Wind(heading, speed, variability, ramptime);
}

void ScriptCommandsProxy::Set_Rain(float density, float ramptime, bool prime)
{
	if (!m_p) return;
	m_p->Set_Rain(density, ramptime, prime);
}

void ScriptCommandsProxy::Set_Snow(float density, float ramptime, bool prime)
{
	if (!m_p) return;
	m_p->Set_Snow(density, ramptime, prime);
}

void ScriptCommandsProxy::Set_Ash(float density, float ramptime, bool prime)
{
	if (!m_p) return;
	m_p->Set_Ash(density, ramptime, prime);
}

void ScriptCommandsProxy::Set_Fog_Enable(bool enabled)
{
	if (!m_p) return;
	m_p->Set_Fog_Enable(enabled);
}

void ScriptCommandsProxy::Set_Fog_Range(float startdistance, float enddistance, float ramptime)
{
	if (!m_p) return;
	m_p->Set_Fog_Range(startdistance, enddistance, ramptime);
}

void ScriptCommandsProxy::Enable_Stealth(GameObject * object, bool onoff)
{
	if (!m_p) return;
	m_p->Enable_Stealth(object, onoff);
}

void ScriptCommandsProxy::Cinematic_Sniper_Control(bool enabled, float zoom)
{
	if (!m_p) return;
	m_p->Cinematic_Sniper_Control(enabled, zoom);
}

int ScriptCommandsProxy::Text_File_Open(const char * filename)
{
	if (!m_p) return 0;
	return m_p->Text_File_Open(filename);
}

bool ScriptCommandsProxy::Text_File_Get_String(int handle, char * buffer, int size)
{
	if (!m_p) return false;
	return m_p->Text_File_Get_String(handle, buffer, size);
}

void ScriptCommandsProxy::Text_File_Close(int handle)
{
	if (!m_p) return;
	m_p->Text_File_Close(handle);
}

void ScriptCommandsProxy::Enable_Vehicle_Transitions(GameObject * object, bool enable)
{
	if (!m_p) return;
	m_p->Enable_Vehicle_Transitions(object, enable);
}

void ScriptCommandsProxy::Display_GDI_Player_Terminal()
{
	if (!m_p) return;
	m_p->Display_GDI_Player_Terminal();
}

void ScriptCommandsProxy::Display_NOD_Player_Terminal()
{
	if (!m_p) return;
	m_p->Display_NOD_Player_Terminal();
}

void ScriptCommandsProxy::Display_Mutant_Player_Terminal()
{
	if (!m_p) return;
	m_p->Display_Mutant_Player_Terminal();
}

bool ScriptCommandsProxy::Reveal_Encyclopedia_Character(int object_id)
{
	if (!m_p) return false;
	return m_p->Reveal_Encyclopedia_Character(object_id);
}

bool ScriptCommandsProxy::Reveal_Encyclopedia_Weapon(int object_id)
{
	if (!m_p) return false;
	return m_p->Reveal_Encyclopedia_Weapon(object_id);
}

bool ScriptCommandsProxy::Reveal_Encyclopedia_Vehicle(int object_id)
{
	if (!m_p) return false;
	return m_p->Reveal_Encyclopedia_Vehicle(object_id);
}

bool ScriptCommandsProxy::Reveal_Encyclopedia_Building(int object_id)
{
	if (!m_p) return false;
	return m_p->Reveal_Encyclopedia_Building(object_id);
}

void ScriptCommandsProxy::Display_Encyclopedia_Event_UI(void)
{
	if (!m_p) return;
	m_p->Display_Encyclopedia_Event_UI();
}

void ScriptCommandsProxy::Scale_AI_Awareness(float sight_scale, float hearing_scale)
{
	if (!m_p) return;
	m_p->Scale_AI_Awareness(sight_scale, hearing_scale);
}

void ScriptCommandsProxy::Enable_Cinematic_Freeze(GameObject * object, bool enable)
{
	if (!m_p) return;
	m_p->Enable_Cinematic_Freeze(object, enable);
}

void ScriptCommandsProxy::Expire_Powerup(GameObject * object)
{
	if (!m_p) return;
	m_p->Expire_Powerup(object);
}

void ScriptCommandsProxy::Set_HUD_Help_Text(int string_id, const Vector3 &color)
{
	if (!m_p) return;
	m_p->Set_HUD_Help_Text(string_id, color);
}

void ScriptCommandsProxy::Enable_HUD_Pokable_Indicator(GameObject * object, bool enable)
{
	if (!m_p) return;
	m_p->Enable_HUD_Pokable_Indicator(object, enable);
}

void ScriptCommandsProxy::Enable_Innate_Conversations(GameObject * object, bool enable)
{
	if (!m_p) return;
	m_p->Enable_Innate_Conversations(object, enable);
}

void ScriptCommandsProxy::Display_Health_Bar(GameObject * object, bool display)
{
	if (!m_p) return;
	m_p->Display_Health_Bar(object, display);
}

void ScriptCommandsProxy::Enable_Shadow(GameObject * object, bool enable)
{
	if (!m_p) return;
	m_p->Enable_Shadow(object, enable);
}

void ScriptCommandsProxy::Clear_Weapons(GameObject * object)
{
	if (!m_p) return;
	m_p->Clear_Weapons(object);
}

void ScriptCommandsProxy::Set_Num_Tertiary_Objectives(int count)
{
	if (!m_p) return;
	m_p->Set_Num_Tertiary_Objectives(count);
}

void ScriptCommandsProxy::Enable_Letterbox(bool onoff, float seconds)
{
	if (!m_p) return;
	m_p->Enable_Letterbox(onoff, seconds);
}

void ScriptCommandsProxy::Set_Screen_Fade_Color(float r, float g, float b, float seconds)
{
	if (!m_p) return;
	m_p->Set_Screen_Fade_Color(r, g, b, seconds);
}

void ScriptCommandsProxy::Set_Screen_Fade_Opacity(float opacity, float seconds)
{
	if (!m_p) return;
	m_p->Set_Screen_Fade_Opacity(opacity, seconds);
}

void ScriptCommandsProxy::Debug_Message(char *format, ...)
{
	if (!m_p) return;
	va_list ap;
	va_start(ap, format);
	char buf[4096];
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	m_p->Debug_Message(buf);
}

void ScriptCommandsProxy::Debug_Message_2(char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char buf[4096];
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	m_p->Debug_Message(buf);
}

