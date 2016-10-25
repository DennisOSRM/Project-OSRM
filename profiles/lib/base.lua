   -- Base class for profile processers.
--
-- Note that LUA has no real class support, but we can emulate them.

local Tags = require('lib/tags')
local Set = require('lib/set')
local Sequence = require('lib/sequence')
local Table = require('lib/table')
local Convert = require('lib/convert')
local Duration = require('lib/duration')
local Access = require("lib/access")

local get_turn_lanes = require("lib/guidance").get_turn_lanes
local set_classification = require("lib/guidance").set_classification


-- Base class and simply class mechanics
Base = {}

function Base:subClass(settings)
  object = self:new()
  return object
end

function Base:new(object)
  object = object or {}
  setmetatable(object, self)
  self.__index = self         -- trick to save memory, see http://lua-users.org/lists/lua-l/2013-04/msg00617.html
  return object
end

-- Each subclasses has it's own settings copy and can overwrite/modify them as needed.
-- A handy way of doing this is by using the utility method to deep merge tables.
Base.settings = {
  name = 'base',
  mode = 'inaccessible',
  oneway_handling = true,
  maxspeed_reduce = true,
  maxspeed_increase = true,
  alternating_oneway_speed_scaling = 0.4,
  use_fallback_names = true,
  ignore_areas = false,
  ignore_buildings = true,
  access = {
    tags = Sequence { 'access' },
    whitelist = Set { 'yes', 'permissive', 'designated', 'destination' },
    blacklist = Set { 'no', 'private', 'agricultural', 'forestry', 'emergency', 'psv','delivery' },
    restricted = Set { 'destination' },
  },
  barrier_whitelist = Set { 'entrance', 'no' },
  way_speeds = {},
  accessible = {},
  routes = {
    route = {
      ferry = { speed = 5, mode = 'ferry' },
      shuttle_train = { speed = 20, mode = 'train' }
    },
    bridge = {
      movable = { speed = 5 },
    },
    railway = {
        train = { speed = 50, mode = 'train', require_access=true },
    },
  },
  nonexisting = Set { 'construction', 'proposed' },
} 

-- Tag helper functions

function Base:tag(key)
  return self.tags:get_value_by_key(key)
end

function Base.present(str)
  return str ~= nil and str ~= ''
end

function Base.empty(str)
  return str == nil or str == ''
end

function Base.extract_int(tag)
  if tag then
    return tonumber(tag:match("%d*"))
  end
end

function Base.convert_to_int(tag)
  if tag then
    return tonumber(tag) or 0
  else
    return 0
  end
end

function Base.is_tag_in_table(tag,table)
  return Base.present(tag) and table and table[tag]
end

function Base.tag_lookup(key,table)
  local tag = self:tag(key)
  return Base.present(tag) and table and table[tag]
end

function Base:is_nonexisting(tag)
  return Base.present(tag) and self.settings.nonexisting[tag]
end

function Base.split_lanes(lanes_string)
  if lanes_string then
    return lanes_string:gmatch("(%w+)")
  else
    return nil
  end
end

-- Main process method entry point

function Base:process(tags,result)
  self:prepare_processing(tags,result)
  if self:pre_process_combined() == false then
    return
  end
  self:process_each_direction(tags,result)
  
  
  -- TODO move to Base class
  local turn_lanes = ""
  local turn_lanes_forward = ""
  local turn_lanes_backward = ""

  turn_lanes, turn_lanes_forward, turn_lanes_backward = get_turn_lanes(tags)
  if  turn_lanes and turn_lanes ~= "" then
    result.turn_lanes_forward = turn_lanes;
    result.turn_lanes_backward = turn_lanes;
  else
    if turn_lanes_forward and turn_lanes_forward ~= ""  then
        result.turn_lanes_forward = turn_lanes_forward;
    end

    if turn_lanes_backward and turn_lanes_backward ~= "" then
        result.turn_lanes_backward = turn_lanes_backward;
    end
  end
  
  
  
  self:post_process_combined()
  self:output(result)
  
  -- TODO move code into Base class and write result to self.tmp.main.common 
  local highway = tags:get_value_by_key("highway")
  set_classification(highway,result,tags)
  
  
  
end

-- Process each direction

function Base:process_each_direction(tags,result)
  self:process_direction(self.tmp.main.forward)
  self:process_direction(self.tmp.main.backward)
end

-- Prepare processsing

function Base:prepare_processing(tags,result)
  self.tags = tags
  self.tmp = {
    main = {
      common = {},
      forward = {
        direction = 'forward',
        debug = {}
      },
      backward = {
        direction = 'backward',
        debug = {}
      }
    }
  }
  self.result = {}
end

-- Convert string to mode code, by accessing global
-- table 'mode' set from C++ (or lua debugging)

function Base.translate_to_mode_code(table,m)
  if table then    -- global table 'mode' found?
    local out = table[m]
    if out then
      return out
    else
      return 0
    end
  else
    return m
  end
end

-- Can we start a route from this way?

function Base:handle_startpoint()
  if self:is_bidirectional() then
    self.tmp.main.common.startpoint = true
  end
end

-- Copy result to the structure that the C++ code expects

function Base:output(result)
 if self.tmp.main.common.name then result.name = self.tmp.main.common.name end
 if self.tmp.main.common.ref then result.ref = self.tmp.main.common.ref end
 if self.tmp.main.common.pronunciation then result.pronunciation = self.tmp.main.common.pronunciation end
 if self.tmp.main.common.roundabout then result.roundabout = self.tmp.main.common.roundabout end
 if self.tmp.main.common.startpoint then result.is_startpoint = self.tmp.main.common.startpoint end
 if self.tmp.main.common.duration then result.duration = self.tmp.main.common.duration end
 
 if self.tmp.main.forward.mode then
   result.forward_mode = Base.translate_to_mode_code( mode,self.tmp.main.forward.mode )
   if self.tmp.main.forward.speed then result.forward_speed = self.tmp.main.forward.speed end
   if self.tmp.main.forward.restricted then result.is_access_restricted = self.tmp.main.forward.restricted end
   if self.tmp.main.forward.turn_lanes then result.turn_lanes_forward = self.tmp.main.forward.turn_lanes end
 else
   result.forward_mode = 0
   result.forward_speed = 0
 end
 
 if self.tmp.main.backward.mode then
   result.backward_mode = Base.translate_to_mode_code( mode,self.tmp.main.backward.mode )
   if self.tmp.main.backward.speed then result.backward_speed = self.tmp.main.backward.speed end
   if self.tmp.main.backward.restricted then result.is_access_restricted = self.tmp.main.backward.restricted end
   if self.tmp.main.backward.turn_lanes then result.turn_lanes_backward = self.tmp.main.backward.turn_lanes end
 else
   result.backward_mode = 0
   result.backward_speed = 0
 end
end

-- Pre-process both directions together

function Base:pre_process_combined()  
  return self:initial_check() ~= false and self:preselect_mode() ~= false
end

-- Post-process both directions together

function Base:post_process_combined()
  self:handle_narrow_ways()
  self:handle_roundabout()
  self:handle_naming()
  self:handle_startpoint()
end

-- Process a single direction (forward or backward)

function Base:process_direction(d)
  self:debug( d, "check " .. self.settings.name )

  if self:handle_access(d) ~= false and
     self:handle_blocking(d) ~= false and
     self:handle_oneway(d) ~= false then
    if self:is_route() == false then
     self:handle_speed(d)
     self:handle_default_speed(d)
     self:handle_speed_adjustment(d)
     --self:handle_turn_lanes(d)
    else
      self:debug(d,'skipping speed for route')
    end
  end
  
  self:handle_mode(d)
end

-- Get status of previously performed route check

function Base:is_route()
  return self.tmp.main.common.route ~= nil
end

-- If the main mode is not possible use this method to try other modes,
-- like pushing bikes (foot) for a bicycle profile.

function Base:handle_alternative_mode(d)
  -- override handle things like pushing bikes
end

-- Determine the mode

function Base:handle_mode(d)
  if not (d.granted or d.implied) then
    d.mode = nil
    if self:is_route() == false then
      self:handle_alternative_mode(d)
    end
  end
end

-- Early check whether something dictates a different mode (like ferry or train)

function Base:preselect_mode()
  self.tmp.main.forward.mode = self.settings.mode
  self.tmp.main.backward.mode = self.settings.mode

  return self:handle_route()
end

-- Intial check to quickly abandon ways that cannot be used for routing

function Base:initial_check()
  if self.settings.ignore_areas and self:tag('area') == 'yes' then
    self:debug( self.tmp.main.forward, "ignoring area")
    self:debug( self.tmp.main.backward, "ignoring area")
    self.tmp.main.common.area = true
    return false
  end
  if self.settings.ignore_buildings and self:tag('building') == 'yes' then
    self:debug( self.tmp.main.forward, "ignoring building")
    self:debug( self.tmp.main.backward, "ignoring building")
    self.tmp.main.common.building = true
    return false
  end
end

-- Check if something is blocking access
-- Note: access tags are handles in another method

function Base:handle_blocking(d)
  if self:is_nonexisting(self:tag('highway')) then
    self:block(d,'not routable','highway')
    return false
  end

  if self:tag('impassable') == 'yes' then
    self:block(d,'blocked','impassable')
    return false
  end

  if self:tag('status') == 'impassable' then
    self:block(d,'blocked','status')
    return false
  end

  if self:tag('smoothness') == 'impassable' then
    self:block(d,'blocked','smoothness')
    return false
  end

  if self:tag('oneway') == 'reversible' then
    self:block(d,'blocked','oneway')
    return false
  end
end

-- Mark as denied/blocked/granted

function Base:deny(d,msg,key,val)
  d.denied = true
  self:debug(d,msg,key,val)
end

function Base:block(d,msg,key,val)
  d.blocked = true
  self:debug(d,msg,key,val)
end


function Base:grant(d,msg,key,val)
  d.granted = true
  self:debug(d,msg,key,val)
end

function Base:is_inaccessible(d)
  return d.denied or d.blocked
end

-- Handle routes like ferry and train

function Base:handle_route()
  for key, settings in pairs(self.settings.routes) do
    if self:try_route(key,settings) then
      return true
    else
      -- construction/proposed denies access, even if access=yes
      local value = self:tag(key)
      if self:is_nonexisting(value) then
        self:debug(self.tmp.main.forward,'nonexisting',key,val)
        self:debug(self.tmp.main.backward,'nonexisting',key,val)
        self.tmp.main.forward.denied = true
        self.tmp.main.backward.denied = true
        return false
      end  
    end      
  end
end

-- Try to handle a specific route, like ferry or train

function Base:try_route(key,settings)
  local value = self:tag(key)
  local r = settings[value]
  if r then
    self.tmp.main.common.route = r
    
    if r.mode then
      self.tmp.main.forward.mode = r.mode
      self.tmp.main.backward.mode = r.mode
    else
      self.tmp.main.forward.mode = self.settings.mode
      self.tmp.main.backward.mode = self.settings.mode
    end
    
    self.tmp.main.forward.implied = true
    self.tmp.main.backward.implied = true
    
    self:debug( self.tmp.main.forward, "using " .. value, key)
    self:debug( self.tmp.main.backward, "using " .. value, key)

    self:handle_duration(r.speed)

    return true
  end
end

-- Handle the duration tag that is used for routes

function Base:handle_duration(speed)
  local duration = Duration.parse( self:tag('duration') )
  if duration then
    self.tmp.main.common.duration = duration
  else
    self.tmp.main.forward.speed = speed
    self.tmp.main.backward.speed = speed
  end
end

-- Handle movable bridges, as a separate mode

function Base:handle_movable_bridge()
  if self:tag('bridge') == 'movable' then
    local mode = self.settings.routes.movable_bridge.mode or self.settings.mode
    local speed = self.settings.routes.movable_bridge.speed
    self.tmp.main.forward.mode = mode
    self.tmp.main.backward.mode = mode
    self:handle_duration(speed)
    return true
  end
end

-- Traverse our access tag hierarchy and return the first key/value pair found

function Base:find_access_tag()
  for i,key in ipairs(self.settings.access.tags) do
    local v = self:tag(key)
    if v then
      return key, v
    end
  end
  return 'access', self:tag('access')
end

-- Traverse our access tag hierarchy and return the first key/value pair found,
-- also looking for directional tags

function Base:find_directional_access_tag(d)
  for v,access_key in ipairs(self.settings.access.tags) do
    local key, tag = self:find_directional_tag( d, access_key )
    if tag then
      return key, tag
    end
  end
  return self:find_directional_tag( d, 'access' )
end

-- For a specific tag key, look for directional tags

function Base:find_directional_tag(d,key)
  local tag = self:tag(key)
  if tag and tag ~= '' then
    return key, tag
  end
  
  if d then
    local direction
    local oneway = self:tag('oneway')
    if oneway == 'reverse' or oneway == '-1' then
      if d.direction == 'forward' then
        direction = 'backward'
      else
        direction = 'forward'
      end
    else
      direction = d.direction
    end
    
    local directional_key = key .. ':' .. direction

    local tag = self:tag(directional_key)
    if tag and tag ~= '' then
      return directional_key, tag
    end
  end
end

-- For a specific base tag, traverse our access tag hierarchy and return 
-- the first key/value pair found. Look for both mode:tag and tag:mode
-- e.g. for a car look for oneway:motorcar, oneway:motor_vehicle, oneway:vehicle and then oneway.
-- Setting abort_at will stop the search when this point is reached in in the access hierarchy,
-- effectively looking only at tags that are at least as specific as that. 

function Base:determine_tag(base_tag, hierarchy, abort_at)
  for i,modifier in ipairs(hierarchy) do
    local key = base_tag .. ':' .. modifier
    local tag = self:tag(key)
    if tag and tag ~= '' then
      return key, tag
    end
    key = modifier .. ':' .. base_tag
    tag = self:tag(key)
    if tag and tag ~= '' then
      return key, tag
    end
    if abort_at and modifier == abort_at then
      return
    end
  end
  local tag = self:tag(base_tag)
  if tag and tag ~= '' then
    return base_tag, tag
  end
end

-- Add a string to the debug message list

function Base:debug(d,str,key,value)
  if key ~= nil and value ~= nil then
    table.insert(d.debug, str .. ' (' .. key .. '=' .. value .. ')')
  elseif key ~= nil then
    table.insert(d.debug, str .. ' (' .. key .. '=' .. self:tag(key) .. ')')
  else
    table.insert(d.debug, str)
  end
end

-- Handle access tags

function Base:handle_access(d)
  local key, access = self:find_directional_access_tag(d)
  d.access_tag = key
  if access then
    if Base.is_tag_in_table( access, self.settings.access.blacklist ) then
      self:debug( d, "access denied", key, access)
      d.denied = true
      d.mode = nil
      return false
    end
    
    if Base.is_tag_in_table( access, self.settings.access.whitelist ) then
      self:debug( d, "access granted", key, access)
      d.granted = true
    end
    
    self:debug( d, access)
    if Base.is_tag_in_table( access, self.settings.access.restricted ) or
       Base.is_tag_in_table( self:tag('service'), self.settings.service_tag_restricted ) then
         self:debug( d, "access restricted", key, access)
      d.restricted = true
    end
  elseif self.tmp.main.common.route then 
    if self.tmp.main.common.route.require_access == true then
      self:debug( d, "access missing")
      d.denied = true
      d.mode = nil
      return false
    end   
  end
end

-- Handle name

function Base:handle_naming()
  self:handle_name()
  self:handle_pronunciation()
  self:handle_ref()
end

function Base:handle_name()
  local name = self:tag('name')
  if Base.present(name) then
    self.tmp.main.common.name = name
  elseif self.settings.use_fallback_names and self:tag('highway') then
    -- If no name or ref present, then encode the highway type, 
    -- so front-ends can at least indicate the type of way to the user.
    -- This encoding scheme is excepted to be a temporary solution.
    self.tmp.main.common.name = '{highway:' .. self:tag('highway') .. '}'
  end
end

-- Handle pronunciation

function Base:handle_pronunciation()
  local pronunciation = self:tag('name:pronunciation')
  if Base.present(pronunciation) then
    self.tmp.main.common.pronunciation = pronunciation
  end
end

-- Handle ref

function Base:handle_ref()
  local ref = self:tag('ref')
  if Base.present(ref) then
    self.tmp.main.common.ref = ref
  end
end

-- Handle roundabout flag

function Base:handle_roundabout()
  if self:tag('junction') == 'roundabout' then
    self.tmp.main.common.roundabout = true
  end
end

-- Determine speed based on way type

function Base:handle_speed(d)
  if d.implied or d.granted then
    return
  end
  
  local speed = self.settings.way_speeds[self:tag('highway')]
  if speed then
    self:debug( d, self.settings.name .." access implied", 'highway' )
    d.implied = true
    self:debug( d, "speed is " .. speed .. " based on way type", 'highway' )
    d.speed = speed
  else
    for k,accepted_values in pairs(self.settings.accessible) do
      local v = self:tag(k)
      if accepted_values[v] then
        self:debug( d, "accessible", k,v )
        d.implied = true
      end
    end
  end
end

-- If nothing more specific was been determined, then use a default speed
  
function Base:handle_default_speed(d)
  if self.tmp.main.common.route then
    if self.tmp.main.common.duration == nil then
      self:debug( d, "using route speed of " .. self.tmp.main.common.route.speed)
      d.speed = self.tmp.main.common.route.speed
    else
      self:debug( d, "duration instead of speed")
    end
  elseif not d.speed then
    if d.granted or d.implied then
      self:debug( d, "using default speed of " .. self.settings.default_speed)
      d.speed = self.settings.default_speed
    end
  end
end

function Base:limit_speed_using_table(d,k,table)
  if d.speed and k and table then
    local v = self:tag(k)
    local max = table[v]
    if v and max then
      self:limit_speed( d, max, k,value )
    end
  end
end

function Base:limit_speed(d,max,k,value)
  if d.speed and max and max>0 and d.speed > max then
    self:debug( d, "limiting speed from " .. d.speed .. " to " .. max, k,value)
    d.speed = max
  end
end

function Base:scale_speed(d,v)
  if d.speed and v then
    d.speed = d.speed * v
  end
end

function Base:increase_speed(d,v)
  if d.speed and v then
    d.speed = d.speed + v
  end
end

-- Adjust speed based on things like surface

function Base:handle_speed_adjustment(d)
  self:handle_maxspeed(d)
  self:handle_side_roads_reduction(d)
  self:handle_alternating_oneway_reduction(d)
  self:limit_speed_using_table( d, 'surface', self.settings.surface_speeds )
  self:limit_speed_using_table( d, 'tracktype', self.settings.tracktype_speeds )
  self:limit_speed_using_table( d, 'smoothness', self.settings.smoothness_speeds )
end

function Base:handle_speed_calibration(d)
end

-- Adjust speed on roads with alternating direction

function Base:handle_alternating_oneway_reduction(d)
  if d.speed and self:tag('oneway') == 'alternating' then
    d.speed = d.speed * self.settings.alternating_oneway_speed_scaling
  end
end

function Base:handle_side_roads_reduction(d)
  if self.settings.side_road_speed_reduction then
    if self:tag('side_road') == 'yes' or self:tag('side_road') == 'rotary' then
      self:debug( d, "reducing speed on side road")
      self:scale_speed( d, self.settings.side_road_speed_reduction )
    end
  end
end

-- Is this a bidirectional way?

function Base:is_bidirectional()
  return self.tmp.main.forward.mode ~= 'inaccessible' and self.tmp.main.backward.mode ~= 'inaccessible'
end

-- Adjust speed on narrow roads

function Base:handle_narrow_ways()
  if self.settings.maxspeed_narrow and self:is_bidirectional() then
    local width = Base.extract_int(self:tag('width'))
    local lanes = Base.extract_int(self:tag('lanes'))
    local key, value
    
    if lanes and lanes==1 then
      key, value = 'lanes', lanes
    elseif width and width<=3 then
      key, value = 'width', width
    end  
    
    if key and value then
      self:debug( self.tmp.main.forward, 'narrow street', key, value)
      self:limit_speed( self.tmp.main.forward, self.settings.maxspeed_narrow )
      self:debug( self.tmp.main.backward, 'narrow street', key, value)
      self:limit_speed( self.tmp.main.backward, self.settings.maxspeed_narrow )
    end
  end
end

-- Estimate speed based on maxspeed value

function Base:maxspeed_to_real_speed(maxspeed)
  local factor = self.settings.maxspeed_factor or 1
  local delta = self.settings.maxspeed_delta or 0
  return (maxspeed * factor) + delta
end

function Base:handle_maxspeed_tag(d,key)
  local value = self:tag(key)
  if value == nil then
    return
  end
  
  local parsed = self:parse_maxspeed( value )
  if parsed == nil then
    return
  end

  local adjusted = self:maxspeed_to_real_speed(parsed)
  if adjusted == nil then
    return
  end
  
  if self.settings.maxspeed_reduce and self.settings.maxspeed_increase then
    self:debug( d, "setting speed to " .. adjusted, key, value)
    d.speed = adjusted
    return true
  elseif self.settings.maxspeed_reduce and (d.speed == nil or adjusted < d.speed) then
    self:debug( d, "reducing speed to " .. adjusted, key, value)
    d.speed = adjusted      
    return true    
  elseif self.settings.maxspeed_increase and (d.speed == nil or adjusted > d.speed) then
    self:debug( d, "increasing speed to " .. adjusted, key, value)
    d.speed = adjusted 
    return true         
  end
end

-- Handle various maxspeed tags

function Base:handle_maxspeed(d)
  if not (self.settings.maxspeed_reduce or self.settings.maxspeed_increase) then
    return
  end
  
  keys = {
    'maxspeed:advisory:' .. d.direction,
    'maxspeed:' .. d.direction,
    'maxspeed:advisory',
    'maxspeed',
  }
  for i,key in ipairs(keys) do
    if self:handle_maxspeed_tag( d, key ) then
      return
    end
  end
end

-- Parse a maxspeed tag value.

function Base:parse_maxspeed(tag)
  if Base.empty(tag) then
    return
  end
  local n = tonumber(tag:match("%d*"))
  if n then
    -- parse direct values like 90, 90 km/h, 40mph
    if string.match(tag, 'mph') or string.match(tag, 'mp/h') then
      n = n * Convert.MilesPerHourToKmPerHour
    end
  else
    -- parse defaults values like FR:urban, using the specified table of defaults and overrides
    if self.settings.maxspeed_defaults then
      tag = string.lower(tag)
      n = self.settings.maxspeed_overrides[tag]
      if not n then
        local highway_type = string.match(tag, "%a%a:(%a+)")
        n = self.settings.maxspeed_defaults[highway_type]
      end
    end
  end
  return n
end

-- Handle oneways

function Base:handle_oneway(d)
  if d.oneway == false then
    return
  end
  if self.settings.oneway_handling == nil or self.settings.oneway_handling == false then
    return
  end
  local key, oneway = self:determine_tag( 'oneway', self.settings.access.tags, self.settings.oneway_handling )
  if d.direction == 'forward' then
    if oneway == '-1' then
      self:debug( d, "access denied by reverse oneway", key, oneway)
      d.denied = true
    end
  elseif d.direction == 'backward' then
    if oneway == '-1' then
      self:debug( d, "not considering implied oneway due to reverse oneway", 'oneway')
    elseif oneway == 'yes' or oneway == '1' or oneway == 'true' then
      self:debug( d, "access denied by explicit oneway", key, oneway)
      d.denied = true
    elseif self.settings.oneway_handling == true and self:is_implied_oneway( d, oneway ) then
      self:implied_oneway(d)
    end    
  end
  if d.denied then
    d.mode = nil
    return false
  end
end

-- Implied oneway found

function Base:implied_oneway(d)
  self:debug( d, "access denied by implied oneway" )
  d.denied = true
end

-- Check if oneway is implied by way type

function Base:is_implied_oneway(d,oneway)
  return self.settings.oneway_handling == true and oneway ~= 'no' and (
    self:tag('junction') == 'roundabout' or
    self:tag('highway') == 'motorway'
  )
end

-- Try processing the direction using an alternative mode.
-- Mode should be a subclass of Base.

function Base:try_alternative_mode(mode,d)
  if mode.initial_check(mode) ~= false then
    
    local alt_direction
    if d.direction == 'forward' then
      alt_direction = mode.tmp.main.forward
    else
      alt_direction = mode.tmp.main.backward
    end

    if mode:preselect_mode() == false then
      return false
    end

    mode:process_direction(alt_direction)
    if alt_direction.mode then
      self:accept_alternative_mode(d,mode,alt_direction)
      return true
    end
  end
end

function Base:accept_alternative_mode(d,alt_mode,alt_direction)
  d.mode = alt_direction.mode
  d.speed = alt_direction.speed
end

-- Main entry point for node processing

function Base:process_node(node, result)
  self.tags = node
  if self:handle_node_access(result) then
    self:handle_node_barriers(result)
  end
  self:handle_node_traffic_lights(result)
end

-- Handle access for node

function Base:handle_node_access(result)
  local key, access = self:find_directional_access_tag()
  if Base.is_tag_in_table( access, self.settings.access.blacklist ) then
    result.barrier = true
  elseif Base.is_tag_in_table( access, self.settings.access.whitelist ) then
  else
    return true
  end
end

-- Handle traffic light flag for node

function Base:handle_node_barriers(result)
  if result.barrier ~= true then
    local barrier = self:tag('barrier')
    if Base.present(barrier) and not Base.is_tag_in_table( barrier, self.settings.barrier_whitelist ) then
      local bollard = self:tag('bollard')
      if self:tag('bollard') ~= 'rising' then
        result.barrier = true
      end
    end
  end
end

-- Handle traffic light flag

function Base:handle_node_traffic_lights(result)
  if self:tag('highway') == "traffic_signals" then
    result.traffic_lights = true
  end
end

-- Compute turn penalty as angle^2 so that sharp turns incur a much higher cost

function Base:turn(angle)
  if self.settings.turn_penalty then
    return angle * angle * self.settings.turn_penalty/(90.0*90.0)
  else
    return 0
  end
end
  
return Base