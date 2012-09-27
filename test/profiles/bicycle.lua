-- Bicycle profile

-- Begin of globals

bollards_whitelist = { [""] = true, ["cattle_grid"] = true, ["border_control"] = true, ["toll_booth"] = true, ["no"] = true, ["sally_port"] = true, ["gate"] = true}
access_tag_whitelist = { ["yes"] = true, ["vehicle"] = true, ["permissive"] = true, ["designated"] = true	}
access_tag_blacklist = { ["no"] = true, ["private"] = true, ["agricultural"] = true, ["forestery"] = true }
access_tag_restricted = { ["destination"] = true, ["delivery"] = true }
access_tags = { "bicycle", "vehicle" }
service_tag_restricted = { ["parking_aisle"] = true }
ignore_in_grid = { ["ferry"] = true }

speed_profile = { 
	["cycleway"] = 18,
	["primary"] = 17,
	["primary_link"] = 17,
	["secondary"] = 18,
	["secondary_link"] = 18,
	["tertiary"] = 18,
	["residential"] = 18,
	["unclassified"] = 16,
	["living_street"] = 16,
	["road"] = 16,
	["service"] = 16,
	["track"] = 13,
	["path"] = 13,
	["footway"] = 5,
	["pedestrian"] = 5,
	["pier"] = 5,
	["steps"] = 1,
	["default"] = 18
}


take_minimum_of_speeds 	= true
obey_oneway 			= true
obey_bollards 			= false
use_restrictions 		= true
ignore_areas 			= true -- future feature
traffic_signal_penalty 	= 2
u_turn_penalty 			= 20

-- End of globals

function node_function (node)
	local barrier = node.tags:Find ("barrier")
	local access = node.tags:Find ("access")
	local traffic_signal = node.tags:Find("highway")
	
	--flag node if it carries a traffic light
	
	if traffic_signal == "traffic_signals" then
	node.traffic_light = true;
	end
	
	if obey_bollards then
		--flag node as unpassable if it black listed as unpassable
		if access_tag_blacklist[barrier] then
		node.bollard = true;
		end
		
		--reverse the previous flag if there is an access tag specifying entrance
		if node.bollard and not bollards_whitelist[barrier] and not access_tag_whitelist[barrier] then
		node.bollard = false;
		end
	end
	return 1
end

function way_function (way, numberOfNodesInWay)
	-- A way must have two nodes or more
	if(numberOfNodesInWay < 2) then
		return 0;
	end
	
	-- First, get the properties of each way that we come across
	local highway = way.tags:Find("highway")
	local name = way.tags:Find("name")
	local ref = way.tags:Find("ref")
	local junction = way.tags:Find("junction")
	local route = way.tags:Find("route")
	local maxspeed = parseMaxspeed(way.tags:Find ( "maxspeed") )
	local man_made = way.tags:Find("man_made")
	local barrier = way.tags:Find("barrier")
	local oneway = way.tags:Find("oneway")
	local onewayClass = way.tags:Find("oneway:bicycle")
	local cycleway = way.tags:Find("cycleway")
	local duration	= way.tags:Find("duration")
	local service	= way.tags:Find("service")
	local area = way.tags:Find("area")
	local access = way.tags:Find("access")
	
		
	-- Set the name that will be used for instructions	
	if "" ~= ref then
		way.name = ref
	elseif "" ~= name then
		way.name = name
	else
		way.name = highway		-- if no name exists, use way type
	end
	
	-- Set the avg speed on the way if it is accessible by road class
	if (speed_profile[highway] ~= nil and way.speed == -1 ) then 
		if (0 < maxspeed and not take_minimum_of_speeds) or (maxspeed == 0) then
			maxspeed = math.huge
		end
		way.speed = math.min(speed_profile[highway], maxspeed)
	end
	
	-- Set the avg speed on ways that are marked accessible
	if access_tag_whitelist[access]	and way.speed == -1 then
		if (0 < maxspeed and not take_minimum_of_speeds) or maxspeed == 0 then
			maxspeed = math.huge
		end
		way.speed = math.min(speed_profile["default"], maxspeed)
	end
	
	-- Set direction according to tags on way
	if obey_oneway then
		if onewayClass == "yes" or onewayClass == "1" or onewayClass == "true" then
			way.direction = Way.oneway
		elseif onewayClass == "no" or onewayClass == "0" or onewayClass == "false" then
			way.direction = Way.bidirectional
		elseif onewayClass == "-1" then
			way.direction = Way.opposite
		elseif oneway == "no" or oneway == "0" or oneway == "false" then
			way.direction = Way.bidirectional
		elseif cycleway == "opposite" or cycleway == "opposite_track" or cycleway == "opposite_lane" then
			way.direction = Way.bidirectional
		elseif oneway == "-1" then
			way.direction = Way.opposite
		elseif oneway == "yes" or oneway == "1" or oneway == "true" or junction == "roundabout" or highway == "motorway_link" or highway == "motorway" then
			way.direction = Way.oneway
		else
			way.direction = Way.bidirectional
		end
	else
		way.direction = Way.bidirectional
	end
	
	way.type = 1
	return 1
end
