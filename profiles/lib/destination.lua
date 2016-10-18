local Destination = {}

function Destination.get_destination(way, is_forward)
  local destination = way:get_value_by_key("destination")
  local destination_ref = way:get_value_by_key("destination:ref")
  local destination_ref_forward = way:get_value_by_key("destination:ref:forward")
  local destination_ref_backward = way:get_value_by_key("destination:ref:backward")
  local destination_forward = way:get_value_by_key("destination:forward")
  local destination_backward = way:get_value_by_key("destination:backward")

  -- Assemble destination as: "A59: Düsseldorf, Köln"
  --          destination:ref  ^    ^  destination

  local rv = ""

  if is_forward == true and destination_ref == "" then
    destination_ref = destination_ref_forward
  elseif is_forward == false then
    destination_ref = destination_ref_backward
  end

  if is_forward == true and destination == "" then
    destination = destination_forward
  elseif is_forward == false then
    destination = destination_backward
  end

  if destination_ref and destination_ref ~= "" then
    rv = rv .. string.gsub(destination_ref, ";", ", ")
  end

  if destination and destination ~= "" then
    if rv ~= "" then
        rv = rv .. ": "
    end

    rv = rv .. string.gsub(destination, ";", ", ")
  end

  return rv
end

return Destination