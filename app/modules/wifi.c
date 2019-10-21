// Module for interfacing with WIFI

// FIXME: sprintf->snprintf everywhere.

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
//#include "aes.h"

#include <string.h>
#include <stddef.h>
#include "ctype.h"

#include <stdint.h>
#include "user_interface.h"
#include "wifi_common.h"


#ifdef WIFI_SMART_ENABLE
#include "smart/smart.h"
#include "smart/smartconfig.h"

static int wifi_smart_succeed = LUA_NOREF;
#endif

static uint8 getap_output_format=0;

#define INVALID_MAC_STR "MAC:FF:FF:FF:FF:FF:FF"

#ifdef WIFI_SMART_ENABLE
static void wifi_smart_succeed_cb(sc_status status, void *pdata){
  NODE_DBG("wifi_smart_succeed_cb is called.\n");

  lua_State* L = lua_getstate();
  if (status == SC_STATUS_LINK_OVER)
  {
    smartconfig_stop();
    return;
  }

#if defined( NODE_SMART_OLDSTYLE )

  if (status != SC_STATUS_LINK || !pdata)
    return;
  if(wifi_smart_succeed == LUA_NOREF)
    return;

  lua_State* L = (lua_State *)arg;
  lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_smart_succeed);
  lua_call(L, 0, 0);

#else

  if (status != SC_STATUS_LINK || !pdata)
    return;

  struct station_config *sta_conf = pdata;
  wifi_station_set_config(sta_conf);
  wifi_station_disconnect();
  wifi_station_connect();

  if(wifi_smart_succeed != LUA_NOREF)
  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_smart_succeed);

    lua_pushstring(L, sta_conf->ssid);
    lua_pushstring(L, sta_conf->password);
    lua_call(L, 2, 0);

    unregister_lua_cb(L, &wifi_smart_succeed);
  }

#endif // defined( NODE_SMART_OLDSTYLE )
}
#endif // WIFI_SMART_ENABLE

static int wifi_scan_succeed = LUA_NOREF;

// callback for wifi_station_listap
static void wifi_scan_done(void *arg, STATUS status)
{
  lua_State* L = lua_getstate();
  uint8 ssid[33];
  char temp[sizeof("11:22:33:44:55:66")];
  if(wifi_scan_succeed == LUA_NOREF)
    return;
  if(arg == NULL)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_scan_succeed);

  if (status == OK)
  {
    struct bss_info *bss_link = (struct bss_info *)arg;
    lua_newtable( L );

    while (bss_link != NULL)
    {
      memset(ssid, 0, 33);
      if (strlen(bss_link->ssid) <= 32)
      {
        memcpy(ssid, bss_link->ssid, strlen(bss_link->ssid));
      }
      else
      {
        memcpy(ssid, bss_link->ssid, 32);
      }
      if(getap_output_format==1) //use new format(BSSID : SSID, RSSI, Authmode, Channel)
      {
        sprintf(temp,MACSTR, MAC2STR(bss_link->bssid));
        wifi_add_sprintf_field(L, temp, "%s,%d,%d,%d",
          ssid, bss_link->rssi, bss_link->authmode, bss_link->channel);
        NODE_DBG(MACSTR" : %s\n",MAC2STR(bss_link->bssid) , temp);//00 00 00 00 00 00
      }
      else //use old format(SSID : Authmode, RSSI, BSSID, Channel)
      {
        wifi_add_sprintf_field(L, ssid, "%d,%d,"MACSTR",%d",
          bss_link->authmode, bss_link->rssi, MAC2STR(bss_link->bssid),bss_link->channel);
        NODE_DBG("%s : %s\n", ssid, temp);
      }

      bss_link = bss_link->next.stqe_next;
    }
  }
  else
  {
    lua_newtable( L );
  }
  lua_call(L, 1, 0);
  unregister_lua_cb(L, &wifi_scan_succeed);
}

#ifdef WIFI_SMART_ENABLE
// Lua: smart(channel, function succeed_cb)
// Lua: smart(type, function succeed_cb)
static int wifi_start_smart( lua_State* L )
{

#if defined( NODE_SMART_OLDSTYLE )

  unsigned channel;
  int stack = 1;

  if ( lua_isnumber(L, stack) )
  {
    channel = lua_tointeger(L, stack);
    stack++;
  }
  else
  {
    channel = 6;
  }

  // luaL_checkanyfunction(L, stack);
  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    if(wifi_smart_succeed != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_smart_succeed);
    wifi_smart_succeed = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if ( channel > 14 || channel < 1 )
    return luaL_error( L, "wrong arg range" );

  if(wifi_smart_succeed == LUA_NOREF){
    smart_begin(channel, NULL, NULL);
  }
  else
  {
    smart_begin(channel, (smart_succeed )wifi_smart_succeed_cb, L);
  }

#else

  if(wifi_get_opmode() != STATION_MODE)
  {
    return luaL_error( L, "Smart link only in STATION mode" );
  }
  uint8_t smart_type = 0;
  int stack = 1;
  if ( lua_isnumber(L, stack) )
  {
    smart_type = lua_tointeger(L, stack);
    stack++;
  }

  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    register_lua_cb(L, &wifi_smart_succeed);
  }

  if ( smart_type > 1 )
    return luaL_error( L, "wrong arg range" );

  smartconfig_set_type(smart_type);
  smartconfig_start(wifi_smart_succeed_cb);

#endif // defined( NODE_SMART_OLDSTYLE )

  return 0;
}

// Lua: exit_smart()
static int wifi_exit_smart( lua_State* L )
{
#if defined( NODE_SMART_OLDSTYLE )
  smart_end();
#else
  smartconfig_stop();
#endif // defined( NODE_SMART_OLDSTYLE )

  unregister_lua_cb(L, &wifi_smart_succeed);
  return 0;
}
#endif // WIFI_SMART_ENABLE

// Lua: wifi.getcountry()
static int wifi_getcountry( lua_State* L ){

  wifi_country_t cfg = {0};


  if(wifi_get_country(&cfg)){
    lua_newtable(L);

    lua_pushstring(L, "country");
    lua_pushstring(L, cfg.cc);
    lua_rawset(L, -3);

    lua_pushstring(L, "start_ch");
    lua_pushnumber(L, cfg.schan);
    lua_rawset(L, -3);

    lua_pushstring(L, "end_ch");
    lua_pushnumber(L, (cfg.schan + cfg.nchan)-1);
    lua_rawset(L, -3);

    lua_pushstring(L, "policy");
    lua_pushnumber(L, cfg.policy);
    lua_rawset(L, -3);

    return 1;
  }
  else{
    return luaL_error(L, "Unable to get country info");
  }
}

// Lua: wifi.setcountry()
static int wifi_setcountry( lua_State* L ){
  size_t len;
  uint8 start_ch;
  uint8 end_ch;
  wifi_country_t cfg = {0};

  if(lua_istable(L, 1)){
    lua_getfield(L, 1, "country");
    if (!lua_isnil(L, -1)){
      if( lua_isstring(L, -1) ){
        const char *country_code = luaL_checklstring( L, -1, &len );
        luaL_argcheck(L, (len==2 && isalpha(country_code[0]) && isalpha(country_code[1])), 1, "country: country code must be 2 chars");
        memcpy(cfg.cc, country_code, len);
        if(cfg.cc[0] >= 0x61) cfg.cc[0]=cfg.cc[0]-32; //if lowercase change to uppercase
        if(cfg.cc[1] >= 0x61) cfg.cc[1]=cfg.cc[1]-32; //if lowercase change to uppercase
      }
      else
        return luaL_argerror( L, 1, "country: must be string" );
    }
    else{
      cfg.cc[0]='C';
      cfg.cc[1]='N';
      cfg.cc[2]=0x00;
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "start_ch");
    if (!lua_isnil(L, -1)){
      if(lua_isnumber(L, -1)){
        start_ch = (uint8)luaL_checknumber(L, -1);
        luaL_argcheck(L, (start_ch >= 1 && start_ch <= 14), 1, "start_ch: Range:1-14");
        cfg.schan = start_ch;
      }
      else
        luaL_argerror(L, 1, "start_ch: must be a number");
    }
    else
      cfg.schan = 1;

    lua_pop(L, 1);

    lua_getfield(L, 1, "end_ch");
    if (!lua_isnil(L, -1)){
      if(lua_isnumber(L, -1)){
        end_ch = (uint8)luaL_checknumber(L, -1);
        luaL_argcheck(L, (end_ch >= 1 && end_ch <= 14), 1, "end_ch: Range:1-14");
        luaL_argcheck(L, (end_ch >= cfg.schan), 1, "end_ch: can't be less than start_ch");
        cfg.nchan = (end_ch-cfg.schan)+1; //cfg.nchan must equal total number of channels
      }
      else
        luaL_argerror(L, 1, "end_ch: must be a number");
    }
    else
      cfg.nchan = (13-cfg.schan)+1;
    lua_pop(L, 1);

    lua_getfield(L, 1, "policy");
    if (!lua_isnil(L, -1)){
      if(lua_isnumber(L, -1)){
        uint8 policy = (uint8)luaL_checknumber(L, -1);
        luaL_argcheck(L, (policy == WIFI_COUNTRY_POLICY_AUTO || policy == WIFI_COUNTRY_POLICY_MANUAL), 1, "policy: must be 0 or 1");
        cfg.policy = policy;
      }
      else
        luaL_argerror(L, 1, "policy: must be a number");
    }
    else
      cfg.policy = WIFI_COUNTRY_POLICY_AUTO;
    lua_pop(L, 1);

    lua_pop(L, 1); //pop table from stack

    bool retval = wifi_set_country(&cfg);
    WIFI_DBG("\n country_cfg:\tcc:\"%s\"\tschan:%d\tnchan:%d\tpolicy:%d\n", cfg.cc, cfg.schan, cfg.nchan, cfg.policy);

    if (retval==1)
      lua_pushboolean(L, true);
    else
      return luaL_error(L, "Unable to set country info");
  }
  else
    return luaL_argerror(L, 1, "Table not found!");
  return 1;
}


int sha1_prf (const u8* key, size_t key_len, const char* label, const char* data, size_t data_len, char* buf, size_t buf_len);

// Lua: wifi.sha1prf(key)
static int wifi_sha1prf( lua_State* L )
{
    size_t len;
    const char *pmk = luaL_checklstring( L, 1, &len );
    luaL_argcheck(L, len==32, 1, INVALID_MAC_STR);

    // Do something with PMK
    const char* label = "Pairwise key expansion";
    char data[76];
    char ptk[64];

    memset(data, 0, sizeof(data));
    sha1_prf(pmk, 32, label, data, 76, ptk, 64);
}

// Lua: wifi.aes(key)
/*static int wifi_aes( lua_State* L )
{
    size_t len;
    const char *key = luaL_checklstring( L, 1, &len );
    luaL_argcheck(L, len==16, 1, INVALID_MAC_STR);

    char data[16];

    memset(data, 0, sizeof(data));

    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);
    AES_ECB_encrypt(&ctx, data);
}*/

// Lua: wifi.setmode(mode, save_to_flash)
static int wifi_setmode( lua_State* L )
{
  unsigned mode;
  bool save_to_flash=true;
  mode = luaL_checkinteger( L, 1 );
  luaL_argcheck(L, mode == STATION_MODE || mode == SOFTAP_MODE || mode == STATIONAP_MODE || mode == NULL_MODE, 1, "Invalid mode");

  if(!lua_isnoneornil(L, 2))
  {
    if(!lua_isboolean(L, 2))
    {
      luaL_typerror(L, 2, lua_typename(L, LUA_TBOOLEAN));
    }
    save_to_flash=lua_toboolean(L, 2);
  }

  if(save_to_flash)
  {
    wifi_set_opmode( (uint8_t)mode);
  }
  else
  {
    wifi_set_opmode_current( (uint8_t)mode);
  }

  mode = (unsigned)wifi_get_opmode();
  lua_pushinteger( L, mode );
  return 1;
}

// Lua: wifi.getmode()
static int wifi_getmode( lua_State* L )
{
  unsigned mode;
  mode = (unsigned)wifi_get_opmode();
  lua_pushinteger( L, mode );
  return 1;
}

// Lua: wifi.getdefaultmode()
static int wifi_getdefaultmode( lua_State* L )
{
  unsigned mode;
  mode = (unsigned)wifi_get_opmode_default();
  lua_pushinteger( L, mode );
  return 1;
}

// Lua: wifi.getchannel()
static int wifi_getchannel( lua_State* L )
{
  unsigned channel;
  channel = (unsigned)wifi_get_channel();
  lua_pushinteger( L, channel );
  return 1;
}

// Lua: wifi.setphymode()
static int wifi_setphymode( lua_State* L )
{
  unsigned mode;

  mode = luaL_checkinteger( L, 1 );

  if ( mode != PHY_MODE_11B && mode != PHY_MODE_11G && mode != PHY_MODE_11N )
    return luaL_error( L, "wrong arg type" );

  wifi_set_phy_mode( (uint8_t)mode);
  mode = (unsigned)wifi_get_phy_mode();
  lua_pushinteger( L, mode );
  return 1;
}

// Lua: wifi.getphymode()
static int wifi_getphymode( lua_State* L )
{
  unsigned mode;
  mode = (unsigned)wifi_get_phy_mode();
  lua_pushinteger( L, mode );
  return 1;
}

// Lua: wifi.setmaxtxpower()
static int wifi_setmaxtxpower( lua_State* L )
{
  unsigned power;
  power = luaL_checkinteger( L, 1 );
  luaL_argcheck(L, (power > 0 && power < 83), 1, "tx power out of range (0->82)");

  system_phy_set_max_tpw( (uint8_t) power);
  return 1;
}

#ifdef PMSLEEP_ENABLE
/* Begin WiFi suspend functions*/
#include <pm/pmSleep.h>

static int wifi_resume_cb_ref = LUA_NOREF; // Holds resume callback reference
static int wifi_suspend_cb_ref = LUA_NOREF; // Holds suspend callback reference

void wifi_pmSleep_suspend_CB(void)
{
  PMSLEEP_DBG("\n\tDBG: %s start\n", __func__);
  if (wifi_suspend_cb_ref != LUA_NOREF)
  {
    lua_State* L = lua_getstate(); // Get main Lua thread pointer
    lua_rawgeti(L, LUA_REGISTRYINDEX, wifi_suspend_cb_ref); // Push suspend callback onto stack
    lua_unref(L, wifi_suspend_cb_ref); // remove suspend callback from LUA_REGISTRY
    wifi_suspend_cb_ref = LUA_NOREF; // Update variable since reference is no longer valid
    lua_call(L, 0, 0); // Execute suspend callback
  }
  else
  {
    PMSLEEP_DBG("\n\tDBG: lua cb unavailable\n");
  }
  PMSLEEP_DBG("\n\tDBG: %s end\n", __func__);
  return;
}

void wifi_pmSleep_resume_CB(void)
{
  PMSLEEP_DBG("\n\tDBG: %s start\n", __func__);
  // If resume callback was defined
  pmSleep_execute_lua_cb(&wifi_resume_cb_ref);
  PMSLEEP_DBG("\n\tDBG: %s end\n", __func__);
  return;
}

// Lua: wifi.suspend({duration, suspend_cb, resume_cb, preserve_mode})
static int wifi_suspend(lua_State* L)
{
  // If no parameters were provided
  if (lua_isnone(L, 1))
  {
    // Return current WiFi suspension state
    lua_pushnumber(L, pmSleep_get_state());
    return 1; // Return WiFi suspension state
  }

  pmSleep_INIT_CFG(cfg);
  cfg.sleep_mode = MODEM_SLEEP_T;
  if(lua_istable(L, 1))
  {
    pmSleep_parse_table_lua(L, 1, &cfg, &wifi_suspend_cb_ref, &wifi_resume_cb_ref);
  }
  else
    return luaL_argerror(L, 1, "must be table");
  cfg.resume_cb_ptr  = &wifi_pmSleep_resume_CB;
  cfg.suspend_cb_ptr = &wifi_pmSleep_suspend_CB;
  pmSleep_suspend(&cfg);
  return 0;
}
// Lua: wifi.resume([Resume_CB])
static int wifi_resume(lua_State* L)
{
  PMSLEEP_DBG("\n\tDBG: %s start\n", __func__);
  uint8 fpm_state = pmSleep_get_state();
// If forced sleep api is not enabled, return error
  if (fpm_state == 0)
  {
      return luaL_error(L, "WIFi not suspended");
  }

  // If a resume callback was provided
  if (lua_isfunction(L, 1))
  {
    // If there is already a resume callback reference
    lua_pushvalue(L, 1); //Push resume callback to the top of the stack
    register_lua_cb(L, &wifi_resume_cb_ref);
    PMSLEEP_DBG("\n\tDBG: Resume CB registered\n");
  }
  pmSleep_resume(NULL);
  PMSLEEP_DBG("\n\tDBG: %s end\n", __func__);
  return 0;
}

/* End WiFi suspend functions*/
#else
static char *susp_note_str = "\n The option \"PMSLEEP_ENABLE\" in \"app/include/user_config.h\" was disabled during FW build!\n";
static char *susp_unavailable_str = "wifi.suspend is unavailable";

static int wifi_suspend(lua_State* L){
  dbg_printf("%s", susp_note_str);
  return luaL_error(L, susp_unavailable_str);
}

static int wifi_resume(lua_State* L){
  dbg_printf("%s", susp_note_str);
  return luaL_error(L, susp_unavailable_str);
}
#endif

// Lua: wifi.nullmodesleep()
static int wifi_null_mode_auto_sleep(lua_State* L)
{
  if (!lua_isnone(L, 1))
  {
    bool auto_sleep_setting=lua_toboolean(L, 1);
    if (auto_sleep_setting!=(bool) get_fpm_auto_sleep_flag())
    {
      wifi_fpm_auto_sleep_set_in_null_mode((uint8)auto_sleep_setting);
      //if esp is already in NULL_MODE, auto sleep setting won't take effect until next wifi_set_opmode(NULL_MODE) call.
      if(wifi_get_opmode()==NULL_MODE)
      {
        wifi_set_opmode_current(NULL_MODE);
      }
    }
  }
  lua_pushboolean(L, (bool) get_fpm_auto_sleep_flag());
  return 1;
}

// Lua: mac = wifi.xx.getmac()
static int wifi_getmac( lua_State* L, uint8_t mode )
{
  char temp[64];
  uint8_t mac[6];
  wifi_get_macaddr(mode, mac);
  sprintf(temp, MACSTR, MAC2STR(mac));
  lua_pushstring( L, temp );
  return 1;
}

// Lua: mac = wifi.xx.setmac()
static int wifi_setmac( lua_State* L, uint8_t mode )
{
  uint8_t mac[6];
  unsigned len = 0;
  const char *macaddr = luaL_checklstring( L, 1, &len );
  luaL_argcheck(L, len==17, 1, INVALID_MAC_STR);
  ets_str2macaddr(mac, macaddr);
  lua_pushboolean(L,wifi_set_macaddr(mode, (uint8 *)mac));
  return 1;
}

// Lua: ip = wifi.xx.getip()
static int wifi_getip( lua_State* L, uint8_t mode )
{
  struct ip_info pTempIp;
  char temp[64];
  wifi_get_ip_info(mode, &pTempIp);
  if(pTempIp.ip.addr==0){
    lua_pushnil(L);
    return 1;
  }
  else
  {
    sprintf(temp, "%d.%d.%d.%d", IP2STR(&pTempIp.ip) );
    lua_pushstring( L, temp );
    sprintf(temp, "%d.%d.%d.%d", IP2STR(&pTempIp.netmask) );
    lua_pushstring( L, temp );
    sprintf(temp, "%d.%d.%d.%d", IP2STR(&pTempIp.gw) );
    lua_pushstring( L, temp );
    return 3;
  }
}

// Lua: broadcast = wifi.xx.getbroadcast()
static int wifi_getbroadcast( lua_State* L, uint8_t mode )
{
  struct ip_info pTempIp;
  char temp[64];
  wifi_get_ip_info(mode, &pTempIp);
  if(pTempIp.ip.addr==0){
    lua_pushnil(L);
    return 1;
  }
  else
  {
    struct ip_addr broadcast_address;

    uint32 subnet_mask32 = pTempIp.netmask.addr & pTempIp.ip.addr;
    uint32 broadcast_address32 = ~pTempIp.netmask.addr | subnet_mask32;
    broadcast_address.addr = broadcast_address32;

    sprintf(temp, "%d.%d.%d.%d", IP2STR(&broadcast_address) );
    lua_pushstring( L, temp );

    return 1;
  }
}

// Used by wifi_setip
static uint32_t parse_key(lua_State* L, const char * key){
  lua_getfield(L, 1, key);
  if( lua_isstring(L, -1) )   // deal with the ip/netmask/gw string
  {
    const char *ip = luaL_checkstring( L, -1 );
    return ipaddr_addr(ip);
  }
  lua_pop(L, 1);
  return 0;
}

// Lua: ip = wifi.xx.setip()
static int wifi_setip( lua_State* L, uint8_t mode )
{
  struct ip_info pTempIp;
  wifi_get_ip_info(mode, &pTempIp);

  if (!lua_istable(L, 1))
    return luaL_error( L, "wrong arg type" );
  uint32_t ip = parse_key(L, "ip");
  if(ip!=0)
    pTempIp.ip.addr = ip;

  ip = parse_key(L, "netmask");
  if(ip!=0)
    pTempIp.netmask.addr = ip;

  ip = parse_key(L, "gateway");
  if(mode==SOFTAP_IF || ip!=0)
    pTempIp.gw.addr = ip;

  if(STATION_IF == mode)
  {
    wifi_station_dhcpc_stop();
    lua_pushboolean(L,wifi_set_ip_info(mode, &pTempIp));
  }
  else
  {
    wifi_softap_dhcps_stop();
    lua_pushboolean(L,wifi_set_ip_info(mode, &pTempIp));
    wifi_softap_dhcps_start();
  }

  return 1;
}

// Lua: wifi.sta.getapinfo
static int wifi_station_get_ap_info4lua( lua_State* L )
{
  struct station_config config[5];
  char temp[sizeof(config[0].password)+1]; //max password length + '\0'
  uint8 number_of_aps = wifi_station_get_ap_info(config);

#if defined(WIFI_DEBUG)
  char debug_temp[128];
#endif
  lua_newtable(L);
  lua_pushnumber(L, number_of_aps);
  lua_setfield(L, -2, "qty");
  WIFI_DBG("\n\t# of APs stored in flash:%d\n", number_of_aps);
  WIFI_DBG(" %-6s %-32s %-64s %-17s\n", "index:", "ssid:", "password:", "bssid:");

  for(int i=0;i<number_of_aps;i++)
  {
    lua_newtable(L);

    memset(temp, 0, sizeof(temp));
    memcpy(temp, config[i].ssid, sizeof(config[i].ssid));
    lua_pushstring(L, temp);
    lua_setfield(L, -2, "ssid");
#if defined(WIFI_DEBUG)
    sprintf(debug_temp, " %-6d %-32s ", i, temp);
#endif

    memset(temp, 0, sizeof(temp));
    if(strlen(config[i].password) > 0) /* WPA = min 8, WEP = min 5 ASCII characters for a 40-bit key */
    {
      memcpy(temp, config[i].password, sizeof(config[i].password));
      lua_pushstring(L, temp);
      lua_setfield(L, -2, "pwd");
    }
#if defined(WIFI_DEBUG)
    sprintf(debug_temp + strlen(debug_temp), "%-64s ", temp);
#endif

    memset(temp, 0, sizeof(temp));
    if (config[i].bssid_set)
    {
      sprintf(temp, MACSTR, MAC2STR(config[i].bssid));
      lua_pushstring(L, temp);
      lua_setfield(L, -2, "bssid");
    }

#if defined(WIFI_DEBUG)
    WIFI_DBG("%s%-17s \n", debug_temp, temp);
#endif
    lua_pushnumber(L, i+1); //Add one, so that AP index follows Lua Conventions
    lua_insert(L, -2);
    lua_settable(L, -3);
  }
  return 1;
}

// Lua: wifi.setapnumber(number_of_aps_to_save)
static int wifi_station_ap_number_set4lua( lua_State* L )
{
  unsigned limit=luaL_checkinteger(L, 1);
  luaL_argcheck(L, (limit >= 1 && limit <= 5), 1, "Valid range: 1-5");
  lua_pushboolean(L, wifi_station_ap_number_set((uint8)limit));
  return 1;
}

// Lua: wifi.setapnumber(number_of_aps_to_save)
static int wifi_station_change_ap( lua_State* L )
{
  uint8 ap_index=luaL_checkinteger(L, 1);
  luaL_argcheck(L, (ap_index >= 1 && ap_index <= 5), 1, "Valid range: 1-5");
  lua_pushboolean(L, wifi_station_ap_change(ap_index-1));
  return 1;
}

// Lua: wifi.setapnumber(number_of_aps_to_save)
static int wifi_station_get_ap_index( lua_State* L )
{
  lua_pushnumber(L, wifi_station_get_current_ap_id()+1);
  return 1;
}

// Lua: wifi.sta.getmac()
static int wifi_station_getmac( lua_State* L ){
  return wifi_getmac(L, STATION_IF);
}

// Lua: wifi.sta.setmac()
static int wifi_station_setmac( lua_State* L ){
  return wifi_setmac(L, STATION_IF);
}

// Lua: wifi.sta.getip()
static int wifi_station_getip( lua_State* L ){
  return wifi_getip(L, STATION_IF);
}

// Lua: wifi.sta.setip()
static int wifi_station_setip( lua_State* L ){
  return wifi_setip(L, STATION_IF);
}

// Lua: wifi.sta.getbroadcast()
static int wifi_station_getbroadcast( lua_State* L ){
  return wifi_getbroadcast(L, STATION_IF);
}

// Used by wifi_station_getconfig_xxx
static int wifi_station_getconfig( lua_State* L, bool get_flash_cfg)
{
  struct station_config sta_conf;
  char temp[sizeof(sta_conf.password)+1]; //max password length + '\0'

  if(get_flash_cfg)
  {
    wifi_station_get_config_default(&sta_conf);
  }
  else
  {
    wifi_station_get_config(&sta_conf);
  }

  if(sta_conf.ssid==0)
  {
    lua_pushnil(L);
      return 1;
  }
  else
  {
    if(lua_isboolean(L, 1) && lua_toboolean(L, 1)==true)
    {
      lua_newtable(L);
      memset(temp, 0, sizeof(temp));
      memcpy(temp, sta_conf.ssid, sizeof(sta_conf.ssid));
      lua_pushstring(L, temp);
      lua_setfield(L, -2, "ssid");

      if(strlen(sta_conf.password) > 0) /* WPA = min 8, WEP = min 5 ASCII characters for a 40-bit key */
      {
        memset(temp, 0, sizeof(temp));
        memcpy(temp, sta_conf.password, sizeof(sta_conf.password));
        lua_pushstring(L, temp);
        lua_setfield(L, -2, "pwd");
      }

      lua_pushboolean(L, sta_conf.bssid_set);
      lua_setfield(L, -2, "bssid_set");

      memset(temp, 0, sizeof(temp));
      sprintf(temp, MACSTR, MAC2STR(sta_conf.bssid));
      lua_pushstring( L, temp);
      lua_setfield(L, -2, "bssid");

      return 1;
    }
    else
    {
      memset(temp, 0, sizeof(temp));
      memcpy(temp, sta_conf.ssid, sizeof(sta_conf.ssid));
      lua_pushstring(L, temp);
      memset(temp, 0, sizeof(temp));
      memcpy(temp, sta_conf.password, sizeof(sta_conf.password));
      lua_pushstring(L, temp);
      lua_pushinteger( L, sta_conf.bssid_set);
      sprintf(temp, MACSTR, MAC2STR(sta_conf.bssid));
      lua_pushstring( L, temp);
      return 4;
    }
  }
}

// Lua: wifi.sta.getconfig()
static int wifi_station_getconfig_current(lua_State *L)
{
  return wifi_station_getconfig(L, false);
}

// Lua: wifi.sta.getdefaultconfig()
static int wifi_station_getconfig_default(lua_State *L)
{
  return wifi_station_getconfig(L, true);
}

// Lua: wifi.sta.clearconfig()
static int wifi_station_clear_config ( lua_State* L )
{
  struct station_config sta_conf;
  bool auto_connect=true;
  bool save_to_flash=true;

  memset(&sta_conf, 0, sizeof(sta_conf));

  wifi_station_disconnect();

  bool config_success;
  if(save_to_flash)
  {
    config_success = wifi_station_set_config(&sta_conf);
  }
  else
  {
    config_success = wifi_station_set_config_current(&sta_conf);
  }

  wifi_station_set_auto_connect((uint8)0);

  lua_pushboolean(L, config_success);
  return 1;
}

// Lua: wifi.sta.config()
static int wifi_station_config( lua_State* L )
{
  struct station_config sta_conf;
  bool auto_connect=true;
  bool save_to_flash=true;
  size_t sl, pl, ml;

  memset(&sta_conf, 0, sizeof(sta_conf));
  sta_conf.threshold.rssi = -127;
  sta_conf.threshold.authmode = AUTH_OPEN;

  if(lua_istable(L, 1))
  {
    lua_getfield(L, 1, "ssid");
    if (!lua_isnil(L, -1))
    {
      if( lua_isstring(L, -1) )
      {
        const char *ssid = luaL_checklstring( L, -1, &sl );
        luaL_argcheck(L, ((sl>=0 && sl<=sizeof(sta_conf.ssid)) ), 1, "ssid: length:0-32"); /* Zero-length SSID is valid as a way to clear config */
        memcpy(sta_conf.ssid, ssid, sl);
      }
      else
      {
        return luaL_argerror( L, 1, "ssid:not string" );
      }
    }
    else
    {
      return luaL_argerror( L, 1, "ssid required" );
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "pwd");
    if (!lua_isnil(L, -1))
    {
      if( lua_isstring(L, -1) )
      {
        const char *pwd = luaL_checklstring( L, -1, &pl );
        luaL_argcheck(L, ((pl>=0 && pl<=sizeof(sta_conf.password)) ), 1, "pwd: length:0-64"); /* WPA = min 8, WEP = min 5 ASCII characters for a 40-bit key */
        memcpy(sta_conf.password, pwd, pl);
      }
      else
      {
        return luaL_argerror( L, 1, "pwd:not string" );
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "bssid");
    if (!lua_isnil(L, -1))
    {
      if (lua_isstring(L, -1))
      {
        const char *macaddr = luaL_checklstring( L, -1, &ml );
        luaL_argcheck(L, ((ml==sizeof("AA:BB:CC:DD:EE:FF")-1) ), 1, "bssid: FF:FF:FF:FF:FF:FF");
        ets_str2macaddr(sta_conf.bssid, macaddr);
        sta_conf.bssid_set = 1;
      }
      else
      {
        return luaL_argerror(L, 1, "bssid:not string");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "auto");
    if (!lua_isnil(L, -1))
    {
      if (lua_isboolean(L, -1))
      {
        auto_connect=lua_toboolean(L, -1);
      }
      else
      {
        return luaL_argerror(L, 1, "auto:not boolean");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "save");
    if (!lua_isnil(L, -1))
    {
      if (lua_isboolean(L, -1))
      {
        save_to_flash=lua_toboolean(L, -1);
      }
      else
      {
        return luaL_argerror(L, 1, "save:not boolean");
      }
    }
    else
    {
      save_to_flash=true;
    }
    lua_pop(L, 1);

#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE

    lua_State* L_temp = NULL;

    lua_getfield(L, 1, "connect_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_STAMODE_CONNECTED);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "connect_cb:not function");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "disconnect_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_STAMODE_DISCONNECTED);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "disconnect_cb:not function");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "authmode_change_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_STAMODE_AUTHMODE_CHANGE);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "authmode_change_cb:not function");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "got_ip_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_STAMODE_GOT_IP);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "gotip_cb:not function");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "dhcp_timeout_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_STAMODE_DHCP_TIMEOUT);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "dhcp_timeout_cb:not function");
      }
    }
    lua_pop(L, 1);

#endif

  }
  else
  {
    return luaL_argerror(L, 1, "config table not found!");
  }

#if defined(WIFI_DEBUG)
  char debug_temp[sizeof(sta_conf.password)+1];  //max password length + '\0'

  memset(debug_temp, 0, sizeof(debug_temp));
  memcpy(debug_temp, sta_conf.ssid, sizeof(sta_conf.ssid));
  WIFI_DBG("\n\tsta_conf.ssid=\"%s\" len=%d\n", debug_temp, sl);

  memset(debug_temp, 0, sizeof(debug_temp));
  memcpy(debug_temp, sta_conf.password, sizeof(sta_conf.password));
  WIFI_DBG("\tsta_conf.password=\"%s\" len=%d\n", debug_temp, pl);
  WIFI_DBG("\tsta_conf.bssid=\""MACSTR"\"\tbssid_set=%d\n", MAC2STR(sta_conf.bssid), sta_conf.bssid_set);
  WIFI_DBG("\tsave_to_flash=%s\n", save_to_flash ? "true":"false");
#endif

  wifi_station_disconnect();

  bool config_success;
  if(save_to_flash)
  {
    config_success = wifi_station_set_config(&sta_conf);
  }
  else
  {
    config_success = wifi_station_set_config_current(&sta_conf);
  }

  wifi_station_set_auto_connect((uint8)auto_connect);
  if(auto_connect)
  {
    wifi_station_connect();
  }

  lua_pushboolean(L, config_success);
  return 1;
}

// Lua: wifi.sta.connect()
static int wifi_station_connect4lua( lua_State* L )
{
#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE
  if(lua_isfunction(L, 1)){
    lua_pushnumber(L, EVENT_STAMODE_CONNECTED);
    lua_pushvalue(L, 1);
    lua_remove(L, 1);
    wifi_event_monitor_register(L);
  }
#endif
  wifi_station_connect();
  return 0;
}

// Lua: wifi.sta.disconnect()
static int wifi_station_disconnect4lua( lua_State* L )
{
#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE
  if(lua_isfunction(L, 1)){
    lua_pushnumber(L, EVENT_STAMODE_DISCONNECTED);
    lua_pushvalue(L, 1);
    lua_remove(L, 1);
    wifi_event_monitor_register(L);
  }
#endif
  wifi_station_disconnect();
  return 0;
}

// Lua: wifi.sta.auto(true/false)
static int wifi_station_setauto( lua_State* L )
{
  unsigned a;

  a = luaL_checkinteger( L, 1 );
  luaL_argcheck(L, ( a == 0 || a == 1 ), 1, "0 or 1");
  wifi_station_set_auto_connect(a);
  return 0;
}

// Lua: wifi.sta.listap()
static int wifi_station_listap( lua_State* L )
{
  if(wifi_get_opmode() == SOFTAP_MODE)
  {
    return luaL_error( L, "Can't list ap in SOFTAP mode" );
  }
  // set safe defaults for scan time, all other members are initialized with 0
  // source: https://github.com/espressif/ESP8266_NONOS_SDK/issues/103
  struct scan_config scan_cfg = {.scan_time = {.passive=120, .active = {.max=120, .min=60}}};

  getap_output_format=0;

  if (lua_type(L, 1)==LUA_TTABLE)
  {
    char ssid[32];
    char bssid[6];
    uint8 channel=0;
    uint8 show_hidden=0;
    size_t len;

    lua_getfield(L, 1, "ssid");
    if (!lua_isnil(L, -1)) /* found? */
    {
      if( lua_isstring(L, -1) )   // deal with the ssid string
      {
        const char *ssidstr = luaL_checklstring( L, -1, &len );
        if(len>32)
          return luaL_error( L, "ssid:<32" );
        memset(ssid, 0, 32);
        memcpy(ssid, ssidstr, len);
        scan_cfg.ssid=ssid;
        NODE_DBG(scan_cfg.ssid);
        NODE_DBG("\n");
      }
      else
      {
        return luaL_error( L, "wrong arg type" );
      }
    }

    lua_getfield(L, 1, "bssid");
    if (!lua_isnil(L, -1)) /* found? */
    {
      if( lua_isstring(L, -1) )   // deal with the ssid string
      {
        const char *macaddr = luaL_checklstring( L, -1, &len );
        luaL_argcheck(L, len==17, 1, INVALID_MAC_STR);
        memset(bssid, 0, 6);
        ets_str2macaddr(bssid, macaddr);
        scan_cfg.bssid=bssid;
        NODE_DBG(MACSTR, MAC2STR(scan_cfg.bssid));
        NODE_DBG("\n");

      }
      else
      {
        return luaL_error( L, "wrong arg type" );
      }
    }


    lua_getfield(L, 1, "channel");
    if (!lua_isnil(L, -1))  /* found? */
    {
      if( lua_isnumber(L, -1) )   // deal with the ssid string
      {
        channel = luaL_checknumber( L, -1);
        if(!(channel>=0 && channel<=13))
          return luaL_error( L, "channel: 0 or 1-13" );
        scan_cfg.channel=channel;
        NODE_DBG("%d\n", scan_cfg.channel);
      }
      else
      {
        return luaL_error( L, "wrong arg type" );
      }
    }

    lua_getfield(L, 1, "show_hidden");
    if (!lua_isnil(L, -1)) /* found? */
    {
      if( lua_isnumber(L, -1) )   // deal with the ssid string
      {
        show_hidden = luaL_checknumber( L, -1);
        if(show_hidden!=0 && show_hidden!=1)
          return luaL_error( L, "show_hidden: 0 or 1" );
        scan_cfg.show_hidden=show_hidden;
        NODE_DBG("%d\n", scan_cfg.show_hidden);

      }
      else
      {
        return luaL_error( L, "wrong arg type" );
      }
    }

    if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION)
    {
      lua_pushnil(L);
      lua_insert(L, 2);
    }
    lua_pop(L, -4);
  }
  else if (lua_type(L, 1) == LUA_TNUMBER)
  {
    lua_pushnil(L);
    lua_insert(L, 1);
  }
  else if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION)
  {
    lua_pushnil(L);
    lua_insert(L, 1);
    lua_pushnil(L);
    lua_insert(L, 1);
  }
  else if(lua_isnil(L, 1))
  {
    if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION)
    {
      lua_pushnil(L);
      lua_insert(L, 2);
    }
  }
  else
  {
    return luaL_error( L, "wrong arg type" );
  }


  if (lua_type(L, 2) == LUA_TNUMBER) //this section changes the output format
  {
    getap_output_format=luaL_checkinteger( L, 2 );
    if (getap_output_format != 0 && getap_output_format != 1)
      return luaL_error( L, "wrong arg type" );
  }
  NODE_DBG("Use alternate output format: %d\n", getap_output_format);
  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    register_lua_cb(L, &wifi_scan_succeed);

    if (lua_type(L, 1)==LUA_TTABLE)
    {
      wifi_station_scan(&scan_cfg,wifi_scan_done);
    }
    else
    {
      wifi_station_scan(NULL,wifi_scan_done);
    }
  }
  else
  {
    unregister_lua_cb(L, &wifi_scan_succeed);
  }
  return 0;
}

// Lua: wifi.sta.gethostname()
static int wifi_sta_gethostname( lua_State* L )
{
  char* hostname = wifi_station_get_hostname();
  lua_pushstring(L, hostname);
  return 1;
}

// Used by wifi_sta_sethostname_lua and wifi_change_default_hostname
// This function checks host name to ensure that it follows RFC 952 & RFC 1123 host name standards.
static bool wifi_sta_checkhostname(const char *hostname, size_t len)
{
  //the hostname must be 32 chars or less and first and last char must be alphanumeric
  if (len == 0 || len > 32 || !isalnum(hostname[0]) || !isalnum(hostname[len-1])){
    return false;
  }
  //characters in the middle of the host name must be alphanumeric or a hyphen(-) only
  for (int i=1; i<len; i++){
    if (!(isalnum(hostname[i]) || hostname[i]=='-')){
      return false;
    }
  }

  return true;
}

// Lua: wifi.sta.sethostname()
static int wifi_sta_sethostname_lua( lua_State* L )
{
  size_t len;
  const char *hostname = luaL_checklstring(L, 1, &len);
  luaL_argcheck(L, wifi_sta_checkhostname(hostname, len), 1, "Invalid hostname");
  lua_pushboolean(L, wifi_station_set_hostname((char*)hostname));
  return 1;
}

// Lua: wifi.sta.sleeptype(type)
static int wifi_station_sleeptype( lua_State* L )
{
  unsigned type;

  if ( lua_isnumber(L, 1) )
  {
    type = lua_tointeger(L, 1);
    luaL_argcheck(L, (type == NONE_SLEEP_T || type == LIGHT_SLEEP_T || type == MODEM_SLEEP_T), 1, "range:0-2");
    if(!wifi_set_sleep_type(type)){
      lua_pushnil(L);
      return 1;
    }
  }

  type = wifi_get_sleep_type();
  lua_pushinteger( L, type );
  return 1;
}

// Lua: wifi.sta.status()
static int wifi_station_status( lua_State* L )
{
  uint8_t status = wifi_station_get_connect_status();
  lua_pushinteger( L, status );
  return 1;
}

// Lua: wifi.sta.getrssi()
static int wifi_station_getrssi( lua_State* L ){
  sint8 rssival=wifi_station_get_rssi();
  NODE_DBG("\n\tRSSI is %d\n", rssival);
  if (rssival<10)
  {
    lua_pushinteger(L, rssival);
  }
  else
  {
    lua_pushnil(L);
  }
  return 1;
}

//Lua: wifi.ap.deauth()
static int wifi_ap_deauth( lua_State* L )
{
  uint8_t mac[6];
  unsigned len = 0;
  if(lua_isstring(L, 1))
  {
    const char *macaddr = luaL_checklstring( L, 1, &len );
    luaL_argcheck(L, len==17, 1, INVALID_MAC_STR);
    ets_str2macaddr(mac, macaddr);
  }
  else
  {
    memset(&mac, 0xFF, sizeof(mac));
  }
  lua_pushboolean(L,wifi_softap_deauth(mac));
  return 1;
}

// Lua: wifi.ap.getmac()
static int wifi_ap_getmac( lua_State* L ){
  return wifi_getmac(L, SOFTAP_IF);
}

// Lua: wifi.ap.setmac()
static int wifi_ap_setmac( lua_State* L ){
  return wifi_setmac(L, SOFTAP_IF);
}

// Lua: wifi.ap.getip()
static int wifi_ap_getip( lua_State* L ){
  return wifi_getip(L, SOFTAP_IF);
}

// Lua: wifi.ap.setip()
static int wifi_ap_setip( lua_State* L ){
  return wifi_setip(L, SOFTAP_IF);
}

// Lua: wifi.ap.getbroadcast()
static int wifi_ap_getbroadcast( lua_State* L ){
  return wifi_getbroadcast(L, SOFTAP_IF);
}

// Lua: wifi.ap.getconfig()
static int wifi_ap_getconfig( lua_State* L, bool get_flash_cfg)
{
  struct softap_config config;
  char temp[sizeof(config.password)+1]; //max password length + '\0'
  if (get_flash_cfg)
  {
    wifi_softap_get_config_default(&config);
  }
  else
  {
    wifi_softap_get_config(&config);
  }

  if(lua_isboolean(L, 1) && lua_toboolean(L, 1)==true)
  {
    lua_newtable(L);

    memset(temp, 0, sizeof(temp));
    memcpy(temp, config.ssid, sizeof(config.ssid));
    lua_pushstring(L, temp);
    lua_setfield(L, -2, "ssid");
    if(config.authmode!=AUTH_OPEN)
    {
      memset(temp, 0, sizeof(temp));
      memcpy(temp, config.password, sizeof(config.password));
      lua_pushstring(L, temp);
      lua_setfield(L, -2, "pwd");
    }
    lua_pushnumber(L, config.authmode);
    lua_setfield(L, -2, "auth");
    lua_pushnumber(L, config.channel);
    lua_setfield(L, -2, "channel");
    lua_pushboolean(L, (bool)config.ssid_hidden);
    lua_setfield(L, -2, "hidden");
    lua_pushnumber(L, config.max_connection);
    lua_setfield(L, -2, "max");
    lua_pushnumber(L, config.beacon_interval);
    lua_setfield(L, -2, "beacon");
    return 1;
  }
  else
  {
    memset(temp, 0, sizeof(temp));
    memcpy(temp, config.ssid, sizeof(config.ssid));
    lua_pushstring(L, temp);

    if(config.authmode == AUTH_OPEN)
    {
      lua_pushnil(L);
    }
    else
    {
      memset(temp, 0, sizeof(temp));
      memcpy(temp, config.password, sizeof(config.password));
      lua_pushstring(L, temp);
    }
  return 2;
  }
}

// Lua: wifi.sta.getconfig()
static int wifi_ap_getconfig_current(lua_State *L)
{
  return wifi_ap_getconfig(L, false);
}

// Lua: wifi.sta.getdefaultconfig()
static int wifi_ap_getconfig_default(lua_State *L)
{
  return wifi_ap_getconfig(L, true);
}

// Lua: wifi.ap.config(table)
static int wifi_ap_config( lua_State* L )
{
  if (!lua_istable(L, 1))
  {
    return luaL_typerror(L, 1, lua_typename(L, LUA_TTABLE));
  }

  struct softap_config config;
  bool save_to_flash=true;
  size_t sl = 0 , pl = 0;
  lua_Integer lint=0;
  int Ltype_tmp=LUA_TNONE;

  memset(config.ssid, 0, sizeof(config.ssid));
  memset(config.password, 0, sizeof(config.password));

  lua_getfield(L, 1, "ssid");
  if (!lua_isnil(L, -1)) /* found? */
  {
    if( lua_isstring(L, -1) )   // deal with the ssid string
    {
      const char *ssid = luaL_checklstring( L, -1, &sl );
      luaL_argcheck(L, ((sl>=1 && sl<=sizeof(config.ssid)) ), 1, "ssid: length:1-32");
      memcpy(config.ssid, ssid, sl);
      config.ssid_len = sl;
      config.ssid_hidden = 0;
    }
    else
    {
      return luaL_argerror( L, 1, "ssid: not string" );
    }
  }
  else
  {
    return luaL_argerror( L, 1, "ssid: required" );
  }
  lua_pop(L, 1);


  lua_getfield(L, 1, "pwd");
  if (!lua_isnil(L, -1)) /* found? */
  {
    if( lua_isstring(L, -1) )   // deal with the password string
    {
      const char *pwd = luaL_checklstring( L, -1, &pl );
      luaL_argcheck(L, (pl>=8 && pl<=sizeof(config.password)), 1, "pwd: length:0 or 8-64");
      memcpy(config.password, pwd, pl);
      config.authmode = AUTH_WPA_WPA2_PSK;
    }
    else
    {
      return luaL_argerror( L, 1, "pwd: not string" );
    }
  }
  else
  {
    config.authmode = AUTH_OPEN;
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "auth");
  if (!lua_isnil(L, -1))
  {
    if(lua_isnumber(L, -1))
    {
      lint=luaL_checkinteger(L, -1);
      luaL_argcheck(L, (lint >= 0 && lint < AUTH_MAX), 1, "auth: Range:0-4");
      config.authmode = (uint8_t)luaL_checkinteger(L, -1);
    }
    else
    {
      return luaL_argerror(L, 1, "auth: not number");
    }

  }
  lua_pop(L, 1);


  lua_getfield(L, 1, "channel");
  if (!lua_isnil(L, -1))
  {
    if(lua_isnumber(L, -1))
    {
      lint=luaL_checkinteger(L, -1);
      luaL_argcheck(L, (lint >= 1 && lint <= 13), 1, "channel: Range:1-13");
      config.channel = (uint8_t)lint;
    }
    else
    {
      luaL_argerror(L, 1, "channel: not number");
    }
  }
  else
  {
    config.channel = 6;
  }
  lua_pop(L, 1);


  lua_getfield(L, 1, "hidden");
  if (!lua_isnil(L, -1))
  {
    Ltype_tmp=lua_type(L, -1);
    if(Ltype_tmp==LUA_TNUMBER||Ltype_tmp==LUA_TBOOLEAN)
    {
      if(Ltype_tmp==LUA_TNUMBER)
      {
        lint=luaL_checkinteger(L, -1);
      }

      if(Ltype_tmp==LUA_TBOOLEAN)
      {
        lint=(lua_Number)lua_toboolean(L, -1);
      }

      luaL_argcheck(L, (lint == 0 || lint==1), 1, "hidden: 0 or 1");
      config.ssid_hidden = (uint8_t)lint;
    }
    else
    {
      return luaL_argerror(L, 1, "hidden: not boolean");
    }
  }
  else
  {
    config.ssid_hidden = 0;
  }
  lua_pop(L, 1);


  lua_getfield(L, 1, "max");
  if (!lua_isnil(L, -1))
  {
    if(lua_isnumber(L, -1))
    {
      lint=luaL_checkinteger(L, -1);
      luaL_argcheck(L, (lint >= 1 && lint <= 4), 1, "max: 1-4");

      config.max_connection = (uint8_t)lint;
    }
    else
    {
      return luaL_argerror(L, 1, "max: not number");
    }
  }
  else
  {
    config.max_connection = 4;
  }
  lua_pop(L, 1);


  lua_getfield(L, 1, "beacon");
  if (!lua_isnil(L, -1))
  {
    if(lua_isnumber(L, -1))
    {
      lint=luaL_checkinteger(L, -1);
      luaL_argcheck(L, (lint >= 100 && lint <= 60000), 1, "beacon: 100-60000");
      config.beacon_interval = (uint16_t)lint;
    }
    else
    {
      return luaL_argerror(L, 1, "beacon: not number");
    }
  }
  else
  {
    config.beacon_interval = 100;
  }
  lua_pop(L, 1);


  lua_getfield(L, 1, "save");
  if (!lua_isnil(L, -1))
  {
    if (lua_isboolean(L, -1))
    {
      save_to_flash=lua_toboolean(L, -1);
    }
    else
    {
      return luaL_argerror(L, 1, "save: not boolean");
    }
  }
  lua_pop(L, 1);

#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE

    lua_State* L_temp = NULL;

    lua_getfield(L, 1, "staconnected_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_SOFTAPMODE_STACONNECTED);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "staconnected_cb:not function");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "stadisconnected_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_SOFTAPMODE_STADISCONNECTED);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "stadisconnected_cb:not function");
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "probereq_cb");
    if (!lua_isnil(L, -1))
    {
      if (lua_isfunction(L, -1))
      {
          L_temp = lua_newthread(L);
          lua_pushnumber(L, EVENT_SOFTAPMODE_PROBEREQRECVED);
          lua_pushvalue(L, -3);
          lua_xmove(L, L_temp, 2);
          wifi_event_monitor_register(L_temp);
      }
      else
      {
        return luaL_argerror(L, 1, "probereq_cb:not function");
      }
    }
    lua_pop(L, 1);

#endif

#if defined(WIFI_DEBUG)
  char debug_temp[sizeof(config.password)+1];
  memset(debug_temp, 0, sizeof(debug_temp));
  memcpy(debug_temp, config.ssid, sizeof(config.ssid));
  WIFI_DBG("\n\tconfig.ssid=\"%s\" len=%d\n", debug_temp, sl);
  memset(debug_temp, 0, sizeof(debug_temp));
  memcpy(debug_temp, config.password, sizeof(config.password));
  WIFI_DBG("\tconfig.password=\"%s\" len=%d\n", debug_temp, pl);
  WIFI_DBG("\tconfig.authmode=%d\n", config.authmode);
  WIFI_DBG("\tconfig.channel=%d\n", config.channel);
  WIFI_DBG("\tconfig.ssid_hidden=%d\n", config.ssid_hidden);
  WIFI_DBG("\tconfig.max_connection=%d\n", config.max_connection);
  WIFI_DBG("\tconfig.beacon_interval=%d\n", config.beacon_interval);
  WIFI_DBG("\tsave_to_flash=%s\n", save_to_flash ? "true":"false");
#endif

  bool config_success;
  if(save_to_flash)
  {
    config_success = wifi_softap_set_config(&config);
  }
  else
  {
    config_success = wifi_softap_set_config_current(&config);
  }

  lua_pushboolean(L, config_success);
  return 1;
}

// Lua: table = wifi.ap.getclient()
static int wifi_ap_listclient( lua_State* L )
{
  if (wifi_get_opmode() == STATION_MODE)
  {
    return luaL_error( L, "Can't list clients in STATION mode" );
  }

  char temp[64];

  lua_newtable(L);

  struct station_info * station = wifi_softap_get_station_info();
  struct station_info * next_station;
  while (station != NULL)
  {
    sprintf(temp, MACSTR, MAC2STR(station->bssid));
    wifi_add_sprintf_field(L, temp, IPSTR, IP2STR(&station->ip));
    station = STAILQ_NEXT(station, next);
  }
  wifi_softap_free_station_info();

  return 1;
}


// Lua: ip = wifi.ap.dhcp.config()
static int wifi_ap_dhcp_config( lua_State* L )
{
  if (!lua_istable(L, 1))
    return luaL_error( L, "wrong arg type" );

  struct dhcps_lease lease;
  uint32_t ip;

  ip = parse_key(L, "start");
  if (ip == 0)
    return luaL_error( L, "wrong arg type" );

  lease.start_ip.addr = ip;
  NODE_DBG(IPSTR, IP2STR(&lease.start_ip));
  NODE_DBG("\n");

  // use configured max_connection to determine end
  struct softap_config config;
  wifi_softap_get_config(&config);
  lease.end_ip = lease.start_ip;
  ip4_addr4(&lease.end_ip) += config.max_connection - 1;

  char temp[64];
  sprintf(temp, IPSTR, IP2STR(&lease.start_ip));
  lua_pushstring(L, temp);
  sprintf(temp, IPSTR, IP2STR(&lease.end_ip));
  lua_pushstring(L, temp);

  // note: DHCP max range = 101 from start_ip to end_ip
  wifi_softap_dhcps_stop();
  wifi_softap_set_dhcps_lease(&lease);
  wifi_softap_dhcps_start();

  return 2;
}


// Lua: wifi.ap.dhcp.start()
static int wifi_ap_dhcp_start( lua_State* L )
{
  lua_pushboolean(L, wifi_softap_dhcps_start());
  return 1;
}


// Lua: wifi.ap.dhcp.stop()
static int wifi_ap_dhcp_stop( lua_State* L )
{
  lua_pushboolean(L, wifi_softap_dhcps_stop());
  return 1;
}


// Module function map
LROT_BEGIN(wifi_station)
  LROT_FUNCENTRY( autoconnect, wifi_station_setauto )
  LROT_FUNCENTRY( changeap, wifi_station_change_ap )
  LROT_FUNCENTRY( clearconfig, wifi_station_clear_config )
  LROT_FUNCENTRY( config, wifi_station_config )
  LROT_FUNCENTRY( connect, wifi_station_connect4lua )
  LROT_FUNCENTRY( disconnect, wifi_station_disconnect4lua )
  LROT_FUNCENTRY( getap, wifi_station_listap )
  LROT_FUNCENTRY( getapindex, wifi_station_get_ap_index )
  LROT_FUNCENTRY( getapinfo, wifi_station_get_ap_info4lua )
  LROT_FUNCENTRY( getbroadcast, wifi_station_getbroadcast )
  LROT_FUNCENTRY( getconfig, wifi_station_getconfig_current )
  LROT_FUNCENTRY( getdefaultconfig, wifi_station_getconfig_default )
  LROT_FUNCENTRY( gethostname, wifi_sta_gethostname )
  LROT_FUNCENTRY( getip, wifi_station_getip )
  LROT_FUNCENTRY( getmac, wifi_station_getmac )
  LROT_FUNCENTRY( getrssi, wifi_station_getrssi )
  LROT_FUNCENTRY( setaplimit, wifi_station_ap_number_set4lua )
  LROT_FUNCENTRY( sethostname, wifi_sta_sethostname_lua )
  LROT_FUNCENTRY( setip, wifi_station_setip )
  LROT_FUNCENTRY( setmac, wifi_station_setmac )
  LROT_FUNCENTRY( sleeptype, wifi_station_sleeptype )
  LROT_FUNCENTRY( status, wifi_station_status )
LROT_END( wifi_station, wifi_station, 0 )


LROT_BEGIN(wifi_ap_dhcp)
  LROT_FUNCENTRY( config, wifi_ap_dhcp_config )
  LROT_FUNCENTRY( start, wifi_ap_dhcp_start )
  LROT_FUNCENTRY( stop, wifi_ap_dhcp_stop )
LROT_END( wifi_ap_dhcp, wifi_ap_dhcp, 0 )


LROT_BEGIN(wifi_ap)
  LROT_FUNCENTRY( config, wifi_ap_config )
  LROT_FUNCENTRY( deauth, wifi_ap_deauth )
  LROT_FUNCENTRY( getip, wifi_ap_getip )
  LROT_FUNCENTRY( setip, wifi_ap_setip )
  LROT_FUNCENTRY( getbroadcast, wifi_ap_getbroadcast )
  LROT_FUNCENTRY( getmac, wifi_ap_getmac )
  LROT_FUNCENTRY( setmac, wifi_ap_setmac )
  LROT_FUNCENTRY( getclient, wifi_ap_listclient )
  LROT_FUNCENTRY( getconfig, wifi_ap_getconfig_current )
  LROT_FUNCENTRY( getdefaultconfig, wifi_ap_getconfig_default )
  LROT_TABENTRY( dhcp, wifi_ap_dhcp )
//  LROT_TABENTRY( __metatable, wifi_ap )
LROT_END( wifi_ap, wifi_ap, 0 )


LROT_BEGIN(wifi)
  LROT_FUNCENTRY( sha1prf, wifi_sha1prf ) // New
  //LROT_FUNCENTRY( aes, wifi_aes ) // New
  LROT_FUNCENTRY( setmode, wifi_setmode )
  LROT_FUNCENTRY( getmode, wifi_getmode )
  LROT_FUNCENTRY( getdefaultmode, wifi_getdefaultmode )
  LROT_FUNCENTRY( getchannel, wifi_getchannel )
  LROT_FUNCENTRY( getcountry, wifi_getcountry )
  LROT_FUNCENTRY( setcountry, wifi_setcountry )
  LROT_FUNCENTRY( setphymode, wifi_setphymode )
  LROT_FUNCENTRY( getphymode, wifi_getphymode )
  LROT_FUNCENTRY( setmaxtxpower, wifi_setmaxtxpower )
  LROT_FUNCENTRY( suspend, wifi_suspend )
  LROT_FUNCENTRY( resume, wifi_resume )
  LROT_FUNCENTRY( nullmodesleep, wifi_null_mode_auto_sleep )
#ifdef WIFI_SMART_ENABLE
  LROT_FUNCENTRY( startsmart, wifi_start_smart )
  LROT_FUNCENTRY( stopsmart, wifi_exit_smart )
#endif
  LROT_FUNCENTRY( sleeptype, wifi_station_sleeptype )

  LROT_TABENTRY( sta, wifi_station )
  LROT_TABENTRY( ap, wifi_ap )
#if defined(WIFI_SDK_EVENT_MONITOR_ENABLE)
  LROT_TABENTRY( eventmon, wifi_event_monitor )
#endif
#if defined(LUA_USE_MODULES_WIFI_MONITOR)
  LROT_TABENTRY( monitor, wifi_monitor )
#endif
  LROT_NUMENTRY( NULLMODE, NULL_MODE )
  LROT_NUMENTRY( STATION, STATION_MODE )
  LROT_NUMENTRY( SOFTAP, SOFTAP_MODE )
  LROT_NUMENTRY( STATIONAP, STATIONAP_MODE )

  LROT_NUMENTRY( PHYMODE_B, PHY_MODE_11B )
  LROT_NUMENTRY( PHYMODE_G, PHY_MODE_11G )
  LROT_NUMENTRY( PHYMODE_N, PHY_MODE_11N )

  LROT_NUMENTRY( NONE_SLEEP, NONE_SLEEP_T )
  LROT_NUMENTRY( LIGHT_SLEEP, LIGHT_SLEEP_T )
  LROT_NUMENTRY( MODEM_SLEEP, MODEM_SLEEP_T )

  LROT_NUMENTRY( OPEN, AUTH_OPEN )
//  LROT_NUMENTRY( WEP, AUTH_WEP )
  LROT_NUMENTRY( WPA_PSK, AUTH_WPA_PSK )
  LROT_NUMENTRY( WPA2_PSK, AUTH_WPA2_PSK )
  LROT_NUMENTRY( WPA_WPA2_PSK, AUTH_WPA_WPA2_PSK )

  LROT_NUMENTRY( STA_IDLE, STATION_IDLE )
  LROT_NUMENTRY( STA_CONNECTING, STATION_CONNECTING )
  LROT_NUMENTRY( STA_WRONGPWD, STATION_WRONG_PASSWORD )
  LROT_NUMENTRY( STA_APNOTFOUND, STATION_NO_AP_FOUND )
  LROT_NUMENTRY( STA_FAIL, STATION_CONNECT_FAIL )
  LROT_NUMENTRY( STA_GOTIP, STATION_GOT_IP )

  LROT_NUMENTRY( COUNTRY_AUTO, WIFI_COUNTRY_POLICY_AUTO )
  LROT_NUMENTRY( COUNTRY_MANUAL, WIFI_COUNTRY_POLICY_MANUAL )

  LROT_TABENTRY( __metatable, wifi )
LROT_END( wifi, wifi, 0 )


// Used by user_rf_pre_init(user_main.c)
void wifi_change_default_host_name(void)
{
  uint8 opmode_temp=wifi_get_opmode();
  wifi_set_opmode_current(STATION_MODE);
  char temp[33] = {0};//32 chars + NULL
  uint8_t mac[6];
  wifi_get_macaddr(STATION_IF, mac);

#ifndef WIFI_STA_HOSTNAME
  sprintf(temp, "NODE-%X%X%X", (mac)[3], (mac)[4], (mac)[5]);
#elif defined(WIFI_STA_HOSTNAME) && !defined(WIFI_STA_HOSTNAME_APPEND_MAC)
  if(wifi_sta_checkhostname(WIFI_STA_HOSTNAME, strlen(WIFI_STA_HOSTNAME))){
    sprintf(temp, "%s", WIFI_STA_HOSTNAME);
  }
  else{
    sprintf(temp, "NODE-%X%X%X", (mac)[3], (mac)[4], (mac)[5]);
  }
#elif defined(WIFI_STA_HOSTNAME) && defined(WIFI_STA_HOSTNAME_APPEND_MAC)
  if(strlen(WIFI_STA_HOSTNAME) <= 26 && wifi_sta_checkhostname(WIFI_STA_HOSTNAME, strlen(WIFI_STA_HOSTNAME))){
    sprintf(temp, "%s%X%X%X", WIFI_STA_HOSTNAME, (mac)[3], (mac)[4], (mac)[5]);
  }
  else{
    sprintf(temp, "NODE-%X%X%X", (mac)[3], (mac)[4], (mac)[5]);
  }
#endif

  wifi_station_set_hostname((char*)temp);

  if(opmode_temp != wifi_get_opmode()){
    wifi_set_opmode_current(opmode_temp);
  }
}

int luaopen_wifi( lua_State *L )
{
  wifi_fpm_auto_sleep_set_in_null_mode(1);
  //if esp is already in NULL_MODE, auto sleep setting won't take effect until next wifi_set_opmode(NULL_MODE) call.
  if(wifi_get_opmode()==NULL_MODE)
  {
    wifi_set_opmode_current(NULL_MODE);
  }
#if defined(WIFI_SDK_EVENT_MONITOR_ENABLE)
  wifi_eventmon_init();
#endif
#if defined(LUA_USE_MODULES_WIFI_MONITOR)
  wifi_monitor_init(L);
#endif
 return 0;
}

NODEMCU_MODULE(WIFI, "wifi", wifi, luaopen_wifi);
