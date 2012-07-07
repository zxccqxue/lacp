#include "lac_base.h"
#include "statmch.h"
#include "lac_sys.h"
#include "lac_pdu.h"

#define STATES {        \
  CHOOSE(INIT),    \
  CHOOSE(SELECTION),    \
}

#define GET_STATE_NAME lac_sel_get_state_name
#include "choose.h"
#if 0
static Bool partners_aport(LAC_PORT_T *aport, LAC_PORT_T *port)
{
   LAC_PORT_T *p = aport;
   do
   {
      if(  (p->actor.system_priority   == port->partner.system_priority     )
        && (!memcmp(p->actor.system_id, port->partner.system_id, 6)           )
        && (p->actor.key               == port->partner.key                 )
        && (p->actor.port_index           == port->partner.port_index             )
        &&((p->actor.port_priority     <  port->actor.port_priority )
          ||( (p->actor.port_priority  == port->actor.port_priority )
            &&(p->actor.port_index        <  port->actor.port_index       )
        )   ) )
        return(True);
   }
   while ((p = p->alink) != aport);
   return(False);
}

/* 
find a port 's aggregator port
aggregator port has the same:{ actor system id, [agg id], actor key, partner system id, partner key}
and has minor{port priority, portno} */
LAC_PORT_T get_dyn_agg_aggregator_port(LAC_PORT_T *port)
{
	   LAC_PORT_T *ap0	= &port->system->ports;
	   LAC_PORT_T *ap 	= &port->system->ports;
	   LAC_PORT_T *best	=  port;
	
	   while ((ap = ap->next) != ap0)
	   {
		  if(  (ap->actor.system_priority	  == port->actor.system_priority	)
			  && (!memcmp(p->actor.system_id, port->actor.system_id))
			&& (ap->actor.key				  == port->actor.key				)
			&& (ap->partner.system_priority   == port->partner.system_priority	)
			  && (!memcmp(p->partner.system_id, port->partner.system_id))
			&& (ap->partner.key 			  == port->partner.key				)
			&& (ap->actor.state.aggregation   &&   ap->partner.state.aggregation)
			&& (port->actor.state.aggregation && port->partner.state.aggregation)
			&&((ap->actor.port_priority 	  <  best->actor.port_priority )
			  ||( (ap->actor.port_priority	  == best->actor.port_priority )
				&&(ap->actor.port_no		  <  best->actor.port_no	   )
			  ) )
			//&& (!partners_aport(ap, port))
			)
			best = ap;
	   }
	   return(best);
	}
	
}
#endif

int select_static_agg_aggregator_port(LAC_PORT_T *port)
{
	LAC_SYS_T *this = port->system;
	LAC_PORT_T *p;
	LAC_PORT_T *best = port;

	/* select best port(master port) */
	for (p = this->ports; p; p = p->next) 
	{
		if(p->agg_id == port->agg_id
			&&(p->actor.system_priority == port->actor.system_priority)
			&& (!memcmp(p->actor.system_id, port->actor.system_id, 6))
			&& p->port_enabled)
		{
			if (p->actor.key > best->actor.key)
				continue;
			
			if (p->actor.key < best->actor.key)
			{
				best = p;
				continue;
			}
			
			if (p->actor.key == best->actor.key)
			{
				if ((p->actor.port_priority < best->actor.port_priority )
					||( (p->actor.port_priority == best->actor.port_priority )
		 				&&(p->actor.port_index < best->actor.port_index)))
				{
					best = p;
				}
			}
		}

	}

	if ((best->partner.system_priority	  == port->partner.system_priority)
		&& (!memcmp(best->partner.system_id, port->partner.system_id, 6))
		&& (best->partner.key == port->partner.key)
		&& (best->actor.state.aggregation && best->partner.state.aggregation)
		&& (port->actor.state.aggregation && port->partner.state.aggregation))
	{
		port->selected = True;
		port->aport = best;
	}
	return 0;
}
int lac_select(LAC_PORT_T *port)
{
	if (port->static_agg)
	{
		select_static_agg_aggregator_port(port);
	}
	else
	{
		//TODO::
	}
	port->reselect = False;
}
void lac_sel_enter_state (LAC_STATE_MACH_T * this)
{
  register LAC_PORT_T *port = this->owner.port;

  switch (this->State) {
    case BEGIN:
    case INIT:
		port->reselect = True;
		port->selected = False;
		break;
		
	case SELECTION:
		lac_select(port);
		break;
		

  };
}

Bool lac_sel_check_conditions (LAC_STATE_MACH_T * this)
{
  register LAC_PORT_T *port = this->owner.port;
  
  switch (this->State) {
    case BEGIN:
	case INIT:
    	return lac_hop_2_state (this, SELECTION);
		
	case SELECTION:		
		if (port->reselect)
			return lac_hop_2_state (this, SELECTION);
		
		break;
		
	default:
		break;
  };
  return False;
}
