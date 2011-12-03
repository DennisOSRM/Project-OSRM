@routing @oneway
Feature: Oneway streets
	Handle oneways streets, as defined at http://wiki.openstreetmap.org/wiki/OSM_tags_for_routing
	
	Scenario: hmm Handle various oneway tag values
 		Given the speedprofile "bicycle"
	 	Then routability should be
		 | highway       | oneway   | forw | backw |
		 | primary       |          | x    | x     |
		 | primary       |          | x    | x     |
		 | primary       |          | x    | x     |
		 | primary       |          | x    | x     |
		 | primary       |          | x    | x     |
		 | primary       | true     | x    |       |

	Scenario: Oneway street
		Given the nodes
		 |  a | b | c |
	
		And the ways
		 | nodes | oneway |
		 | abc   | -1    |
    
		When I route I should get
		 | from | to | route |
		 | a    | c  |    |
		 | c    | a  | abc      |

	Scenario: Around the Block
		Given the nodes
		 | a | b |
		 | d | c |		
	
		And the ways
		 | nodes | oneway |
		 | ab    | yes    |
		 | bc    |        |
		 | cd    |        |
		 | da    |        |
    
		When I route I should get
		 | from | to | route    |
		 | a    | b  | ab       |
		 | b    | a  | bc,cd,da |
	
	Scenario: Avoid oneway traps
		Given the nodes
		 |   | x |   |		
		 | a | b | c |
		 |   | y |   |		

		And the ways
		 | nodes | oneway |
		 | abc   |        |
		 | bx    | yes    |
		 | yb    | -1     |

		When I route I should get
		 | from | to | route |
		 | b    | x  |       |
		 | b    | y  |       |
	
	Scenario: Simplest possible oneway
 		Given the speedprofile "bicycle"
	 	Then routability should be
		 | highway | oneway | forw | backw |
		 | primary | yes    | x    |       |
		
	Scenario: Handle various oneway tag values
 		Given the speedprofile "bicycle"
	 	Then routability should be
		 | highway       | oneway   | forw | backw |
		 | primary       |          | x    | x     |
		 | primary       | nonsense | x    | x     |
		 | primary       | no       | x    | x     |
		 | primary       | false    | x    | x     |
		 | primary       | 0        | x    | x     |
		 | primary       | yes      | x    |       |
		 | primary       | true     | x    |       |
		 | primary       | 1        | x    |       |
		 | primary       | -1       |      | x     |
	
	Scenario: Implied oneways
	 	Given the speedprofile "car"
	 	Then routability should be
		 | highway        | junction   | forw | backw |
		 | motorway       |            | x    |       |
		 | motorway_link  |            | x    |       |
		 | trunk          |            | x    | x     |
		 | trunk_link     |            | x    | x     |
		 | primary        |            | x    | x     |
		 | primary_link   |            | x    | x     |
		 | secondary      |            | x    | x     |
		 | secondary_link |            | x    | x     |
		 | tertiary       |            | x    | x     |
		 | tertiary_link  |            | x    | x     |
		 | residential    |            | x    | x     |
		 | primary        | roundabout | x    |       |
		 | secondary      | roundabout | x    |       |
		 | tertiary       | roundabout | x    |       |
		 | residential    | roundabout | x    |       |

	Scenario: Overriding implied oneways
 		Given the speedprofile "car"
	 	Then routability should be
		 | highway       | junction   | oneway | forw | backw |
		 | motorway_link |            | no     | x    | x     |
		 | trunk_link    |            | no     | x    | x     |
		 | primary       | roundabout | no     | x    | x     |
		 | motorway_link |            | yes    | x    |       |
		 | trunk_link    |            | yes    | x    |       |
		 | primary       | roundabout | yes    | x    |       |
		 | motorway_link |            | -1     |      | x     |
		 | trunk_link    |            | -1     |      | x     |
		 | primary       | roundabout | -1     |      | x     |

	Scenario: Disabling oneways in speedprofile
 		Given the speedprofile "car"
	 	And the speedprofile settings
		 | obeyOneways | no |
		Then routability should be
		 | highway       | junction   | oneway | forw | backw |
		 | primary       |            | yes    | x    | x     |
		 | primary       |            | true   | x    | x     |
		 | primary       |            | 1      | x    | x     |
		 | primary       |            | -1     | x    | x     |
		 | motorway_link |            |        | x    | x     |
		 | trunk_link    |            |        | x    | x     |
		 | primary       | roundabout |        | x    | x     |
	
	@bicycle
	Scenario: Oneway:bicycle should override normal oneways tags
 		Given the speedprofile "bicycle"
	 	Then routability should be
		 | highway       | junction   | oneway | oneway:bicycle | forw | backw |
		 | primary       |            |        | yes            | x    |       |
		 | primary       |            | yes    | yes            | x    |       |
		 | primary       |            | no     | yes            | x    |       |
		 | primary       |            | -1     | yes            | x    |       |
		 | primary       | roundabout |        | yes            | x    |       |
		 | primary       |            |        | no             | x    | x     |
		 | primary       |            | yes    | no             | x    | x     |
		 | primary       |            | no     | no             | x    | x     |
		 | primary       |            | -1     | no             | x    | x     |
		 | primary       | roundabout |        | no             | x    | x     |
		 | primary       |            |        | -1             |      | x     |
		 | primary       |            | yes    | -1             |      | x     |
		 | primary       |            | no     | -1             |      | x     |
		 | primary       |            | -1     | -1             |      | x     |
		 | primary       | roundabout |        | -1             |      | x     |
	
	@bicycle
	Scenario: Bicycles and contra flow
 		Given the speedprofile "bicycle"
	 	Then routability should be
		 | highway | oneway | cycleway       | forw | backw |
		 | primary | yes    | opposite       | x    | x     |
		 | primary | yes    | opposite_track | x    | x     |
		 | primary | yes    | opposite_lane  | x    | x     |
		 | primary | -1     | opposite       | x    | x     |
		 | primary | -1     | opposite_track | x    | x     |
		 | primary | -1     | opposite_lane  | x    | x     |
		 | primary | no     | opposite       | x    | x     |
		 | primary | no     | opposite_track | x    | x     |
		 | primary | no     | opposite_lane  | x    | x     |
	
	@bicycle
	Scenario: Cars should not be affected by bicycle tags
 		Given the speedprofile "car"
		Then routability should be
		 | highway | junction   | oneway | oneway:bicycle | forw | backw |
		 | primary |            | yes    | yes            | x    |       |
		 | primary |            | yes    | no             | x    |       |
		 | primary |            | yes    | -1             | x    |       |
		 | primary |            | no     | yes            | x    | x     |
		 | primary |            | no     | no             | x    | x     |
		 | primary |            | no     | -1             | x    | x     |
		 | primary |            | -1     | yes            |      | x     |
		 | primary |            | -1     | no             |      | x     |
		 | primary |            | -1     | -1             |      | x     |
		 | primary | roundabout |        | yes            | x    |       |
		 | primary | roundabout |        | no             | x    |       |
		 | primary | roundabout |        | -1             | x    |       |
