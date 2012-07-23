#include "lacp_base.h"#include "lacp_statmch.h"#include "lacp_port.h"#include "lacp_sys.h"#include "lacp_api.h"#include "lacp_ssp.h"	uint32_t max_port = 4;
/*****************************************************************************
 函 数 名  : lacp_port_get_actor_admin
 功能描述  : 获取端口的actor配置数据，如果没有，则使用默认值
 输入参数  : uint32_t port_index                  
             lacp_port_info_t  *actor_admin  
 输出参数  : 无
 返 回 值  : unsigned
 调用函数  : 
 被调函数  : 
 
 修改历史      :
  1.日    期   : 2012年7月23日
    作    者   : jiangli
    修改内容   : 新生成函数

*****************************************************************************/
uint32_t lacp_port_get_actor_admin(uint32_t port_index, lacp_port_info_t  *admin)
{
	lacp_mac_t sys_mac;
	
	/* try to get data from db */

	/* if not exist, use default */
    admin->port_priority		   = Default_port_priority;
    admin->port_no		   = port_index + 1;
    admin->system_priority	   = Default_system_priority;
    memcpy(admin->system_mac, sys_mac, 6);
    admin->key				   = Default_key;

    LACP_STATE_SET_BIT(admin->state, LACP_STATE_ACT, Default_lacp_activity);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_TMT, Default_lacp_timeout);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_AGG, Default_aggregation);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_SYN, False);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_DEF, True);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_EXP, False);

	return 0;
}



/*****************************************************************************
 函 数 名  : lacp_port_get_partner_admin
 功能描述  : 获取端口的partner配置数据，如果没有，则使用默认值
 输入参数  : uint32_t port_index            
             lacp_port_info_t  *admin  
 输出参数  : 无
 返 回 值  : unsigned
 调用函数  : 
 被调函数  : 
 
 修改历史      :
  1.日    期   : 2012年7月23日
    作    者   : jiangli
    修改内容   : 新生成函数

*****************************************************************************/
uint32_t lacp_port_get_partner_admin(uint32_t port_index, lacp_port_info_t  *admin)
{
	lacp_mac_t sys_mac;
	
	/* try to get data from db */

	/* if not exist, use default */
    admin->port_priority		 = Default_port_priority;
    admin->port_no				 = port_index;/* TODO:: */
    admin->system_priority		 = Default_system_priority;
    memset(admin->system_mac, 0, 6);
    admin->key					 = port_index;

    LACP_STATE_SET_BIT(admin->state, LACP_STATE_ACT, False); /* Passive	  */
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_TMT, False); /* Long timeout */
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_AGG, False); /* Individual   */
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_SYN, True);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_COL, True);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_DIS, True);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_DEF, True);
    LACP_STATE_SET_BIT(admin->state, LACP_STATE_EXP, False);

	return 0;
}

static lacp_port_t *_lacp_port_find (lacp_sys_t * sys, uint32_t port_index){	register lacp_port_t *port;	for (port = sys->ports; port; port = port->next)		if (port_index == port->port_index) {			return port;		}	return NULL;}uint32_t lacp_rx_lacpdu(uint32_t port_index, lacp_pdu_t * bpdu, uint32_t len){	register lacp_port_t *port;	register lacp_sys_t *sys;	uint32_t iret;	LAC_CRITICAL_PATH_START;	lacp_trace(" port %d rx lacpdu", port_index);	sys = lacp_get_sys_inst();	if (!sys) {		lacp_trace("the instance had not yet been created");		LAC_CRITICAL_PATH_END;		return M_LAC_NOT_CREATED;	}	port = _lacp_port_find (sys, port_index);	if (!port) {			/* port is absent in the stpm :( */		lacp_trace ("RX lacpdu port=%d port is absent :(", (uint32_t) port_index);		LAC_CRITICAL_PATH_END;		return M_RSTP_PORT_IS_ABSENT;	}	if (!port->port_enabled) { /* port link change indication will come later :( */		lacp_trace ("disable port receive lacpdu port=%d :(", (uint32_t) port_index);		LAC_CRITICAL_PATH_END;		return M_RSTP_NOT_ENABLE;	}	if (LAC_ENABLED != sys->admin_state) { /* the stpm had not yet been enabled :( */		LAC_CRITICAL_PATH_END;		return M_RSTP_NOT_ENABLE;	}	iret = lacp_port_rx_lacpdu (port, bpdu, len);	if (iret)	{		lacp_trace("rx process error");		return -1;	}	lacp_sys_update (sys, LACP_SYS_UPDATE_READON_RX);	LAC_CRITICAL_PATH_END;	return iret;}uint32_t lacp_port_set_cfg(lacp_port_cfg_t * uid_cfg){	register lacp_sys_t *sys;	register lacp_port_t *port;	uint32_t port_no;	Bool update_fsm = True;	LAC_CRITICAL_PATH_START;	sys = lacp_get_sys_inst();	for (port_no = 0; port_no < max_port; port_no++) {		if (!lacp_bitmap_get_bit (&uid_cfg->port_bmp, port_no))			continue;		port = _lacp_port_find (sys, port_no);		if (!port) {		  /* port is absent in the stpm :( */			continue;		}		if (uid_cfg->field_mask & PT_CFG_STATE)		{			if (uid_cfg->lacp_enabled)			{				/* add to agg */				LACP_STATE_SET_BIT(port->actor.state, LACP_STATE_AGG, True);				port->agg_id = uid_cfg->agg_id;				lacp_port_set_reselect(port);			} else {			/* delete from agg. */				/* notify to partner*/				port->ntt = True;				LACP_STATE_SET_BIT(port->actor.state, LACP_STATE_AGG, False);				lacp_sys_update (sys, LACP_SYS_UPDATE_READON_PORT_CFG);				/* set agg member reselect */				lacp_port_set_reselect(port);				port->agg_id = 0;			}			port->lacp_enabled = uid_cfg->lacp_enabled;		}		if (uid_cfg->field_mask & PT_CFG_COST)		{			lacp_port_set_reselect(port);		}		if (uid_cfg->field_mask & PT_CFG_STAT)		{			port->rx_lacpdu_cnt = 0;			port->tx_lacpdu_cnt = 0;		}		if (uid_cfg->field_mask & PT_CFG_PRIO)		{			printf("\r\n priority:%d!!!", uid_cfg->port_priority);			port->actor.port_priority = uid_cfg->port_priority;			lacp_port_set_reselect(port);		}	}	if (update_fsm)		lacp_sys_update (sys, LACP_SYS_UPDATE_READON_PORT_CFG);	LAC_CRITICAL_PATH_END;	return 0;}uint32_t lacp_port_get_cfg(uint32_t port_index, lacp_port_cfg_t * uid_cfg){	/* get from db */		return 0;}uint32_t _lacp_copy_port_state(UID_LAC_PORT_STATE_T * uid_cfg, lacp_port_t *port){	uid_cfg->valid = True;	uid_cfg->port_index = port->port_index;	uid_cfg->agg_id = port->agg_id;	uid_cfg->rx_cnt = port->rx_lacpdu_cnt;	uid_cfg->tx_cnt = port->tx_lacpdu_cnt;	uid_cfg->master_port = port->master_port->port_index;	if (port->selected && !port->standby)	{		uid_cfg->sel_state = True;	}	else		uid_cfg->sel_state = False;	memcpy(&uid_cfg->actor, &port->actor, sizeof(lacp_port_info_t));	memcpy(&uid_cfg->partner, &port->partner, sizeof(lacp_port_info_t));	return 0;}uint32_t lacp_agg_get_port_state(uint32_t agg_id, UID_LAC_PORT_STATE_T * uid_cfg, uint32_t *master_index){	register lacp_sys_t *lacp_sys;	register lacp_port_t *port;	uint32_t cnt = 0;	lacp_sys = lacp_get_sys_inst();	for (port = lacp_sys->ports; port; port = port->next)	{		if (port->agg_id != agg_id)		{			continue;		}		if (cnt >= 8)		{			break;		}		_lacp_copy_port_state(&uid_cfg[cnt], port);		if (port->selected && !port->standby && (port->master_port == port))		{			*master_index = cnt;		}		cnt++;	}	return 0;}uint32_t lacp_port_get_port_state(uint32_t port_index, UID_LAC_PORT_STATE_T * uid_cfg){	register lacp_sys_t *lacp_sys;	register lacp_port_t *port;	uint32_t cnt = 0;	lacp_sys = lacp_get_sys_inst();	port = _lacp_port_find (lacp_sys, port_index);	if (!port) {		  /* port is absent in the stpm :( */		return -1;	}	_lacp_copy_port_state(uid_cfg, port);	return 0;}#if 0uint32_t lacp_agg_get_master_port(uint32_t agg_id, lacp_port_cfg_t * uid_cfg){	register lacp_sys_t *lacp_sys;	register lacp_port_t *port;	lacp_sys = lacp_get_sys_inst();	for (port = lacp_sys->ports; port; port = port->next)	{		if (port->agg_id != agg_id)		{			continue;		}		uid_cfg->key = port->aport->key;		uid_cfg->agg_id = port->aport->agg_id;		uid_cfg->lacp_enabled = port->aport->lacp_enabled;		lacp_bitmap_set_bit(uid_cfg->port_bmp, port->aport->port_index);		uid_cfg->sel_state = !port->aport->standby;		memcpy(&uid_cfg->partner, port->aport->partner, sizeof(lacp_port_info_t);)		memcpy(&uid_cfg->partner, port->aport->partner, sizeof(lacp_port_info_t));	}	return 0;}#endifuint32_t lacp_port_get_dbg_cfg(uint32_t port_index, lacp_port_t * port){	register lacp_sys_t *sys;	register lacp_port_t *p;	lacp_trace("\r\n %s.%d",  __FUNCTION__, __LINE__);	sys = lacp_get_sys_inst();	p = _lacp_port_find (sys, port_index);	if (!port) {		  /* port is absent in the stpm :( */		return 1;	}	memcpy(port, p, sizeof(lacp_port_t));	return 0;}voidlacp_one_second (){	lacp_sys_t *sys = lacp_get_sys_inst();	register lacp_port_t *port;	register uint32_t iii;	if (LAC_ENABLED != sys->admin_state)		return;	LAC_CRITICAL_PATH_START;	for (port = sys->ports; port; port = port->next) {		for (iii = 0; iii < TIMERS_NUMBER; iii++) {//			  lacp_trace("\r\n*******************88^^^port:%d iii:%d,value:%d\r\n", port->port_index, iii, *(port->timers[iii]));			if (*(port->timers[iii]) > 0) {				(*port->timers[iii])--;			}		}		port->hold_count = 0;	}	lacp_sys_update (sys, LACP_SYS_UPDATE_READON_TIMER);	LAC_CRITICAL_PATH_END;}uint32_t lacp_create_ports(lacp_bitmap_t *ports){	//TODO:: create port	uint32_t port_index;	lacp_sys_t *lacp_sys;	lacp_sys = lacp_get_sys_inst();	for (port_index = 0; port_index < max_port; port_index++)	{		if (lacp_bitmap_get_bit(ports, port_index))		{			if (lacp_port_create(lacp_sys, port_index))			{				lacp_sys->number_of_ports ++;				lacp_bitmap_set_bit(lacp_sys->portmap, port_index);			}			else			{				printf("\r\n create port fail");				continue;			}		}	}	lacp_sys_update (lacp_sys, LACP_SYS_UPDATE_READON_PORT_CREATE);	return 0;}uint32_t lacp_remove_ports(lacp_bitmap_t *ports){	uint32_t port_index;	lacp_sys_t *lacp_sys;	lacp_port_t *port;	lacp_sys = lacp_get_sys_inst();	for (port_index = 0; port_index < max_port; port_index++)	{		if (lacp_bitmap_get_bit(ports, port_index))		{			port = _lacp_port_find(lacp_sys, port_index);			lacp_port_delete(port);			lacp_sys->number_of_ports --;			lacp_bitmap_clear_bit(lacp_sys->portmap, port_index);		}	}	return 0;}uint32_t lacp_port_link_change(uint32_t port_index, uint32_t link_status){	lacp_sys_t *sys;	lacp_port_t *p;	lacp_trace ("port p%02d => %sABLE", (uint32_t) port_index, link_status ? "EN" : "DIS");	LAC_CRITICAL_PATH_START;	sys = lacp_get_sys_inst();	p = _lacp_port_find (sys, port_index);	if (!p) {		/* port is absent in the stpm :( */		return -1;	}	if (p->port_enabled == link_status) {	/* nothing to do :) */		return;	}	if (link_status)	{		p->port_enabled = True;	}	else	{		p->port_enabled = False;	}	p->speed = lacp_ssp_get_port_oper_speed(p->port_index);	p->duplex = lacp_ssp_get_port_oper_duplex(p->port_index);	if (!p->duplex)	{		LACP_STATE_SET_BIT(p->actor.state, LACP_STATE_AGG, False);	}	lacp_port_set_reselect(p);	lacp_sys_update(sys, LACP_SYS_UPDATE_READON_LINK);	LAC_CRITICAL_PATH_END;	return 0;}static uint32_t _lacp_port_update_info(){	register lacp_sys_t *lacp_sys;	register lacp_port_t *port;	lacp_sys = lacp_get_sys_inst();	for (port = lacp_sys->ports; port; port = port->next)	{		port->actor.system_priority = lacp_sys->priority;		LACP_STATE_SET_BIT(port->actor.state, LACP_STATE_TMT, lacp_sys->lacp_timeout);		port->ntt = True;	}	return 0;}uint32_t lacp_sys_set_cfg(UID_LAC_CFG_T * uid_cfg){	lacp_sys_t *sys = lacp_get_sys_inst();	uint32_t port_loop = 0;	lacp_port_t *p;	LAC_CRITICAL_PATH_START;	if (uid_cfg->field_mask & BR_CFG_PBMP_ADD)	{		for (port_loop = 0; port_loop < max_port; port_loop++)			if (lacp_bitmap_get_bit(&uid_cfg->ports, port_loop))				lacp_port_create(sys, port_loop);	}	if (uid_cfg->field_mask & BR_CFG_PBMP_DEL)	{		for (port_loop = 0; port_loop < max_port; port_loop++)			if (lacp_bitmap_get_bit(&uid_cfg->ports, port_loop))			{				p = _lacp_port_find(sys, port_loop);				lacp_port_delete(p);			}	}	else if (uid_cfg->field_mask & BR_CFG_PRIO)	{		sys->priority = uid_cfg->priority;	}	else if (uid_cfg->field_mask & BR_CFG_LONG_PERIOD)	{		sys->slow_periodic_time = uid_cfg->long_period;	}	else if (uid_cfg->field_mask & BR_CFG_SHORT_PERIOD)	{		sys->fast_periodic_time = uid_cfg->short_period;	}	else if (uid_cfg->field_mask & BR_CFG_PERIOD)	{		sys->lacp_timeout = uid_cfg->period;	}	_lacp_port_update_info();	lacp_sys_update (sys, LACP_SYS_UPDATE_READON_SYS_CFG);	LAC_CRITICAL_PATH_END;	return 0;}uint32_t lacp_sys_get_cfg(UID_LAC_CFG_T * uid_cfg){	lacp_sys_t *sys = lacp_get_sys_inst();	uint32_t port_loop = 0;	lacp_port_t *port;	uid_cfg->number_of_ports = max_port;	for (port = sys->ports; port; port = port->next)	{		lacp_bitmap_set_bit(&uid_cfg->ports, port->port_index);	}		uid_cfg->priority = sys->priority;	memcpy(uid_cfg->sys_mac, sys->mac, 6);	uid_cfg->short_period = sys->fast_periodic_time;	uid_cfg->long_period = sys->slow_periodic_time;	return 0;}