@routing @bad
Feature: Handle bad data in a graceful manner
	
	Scenario: Empty dataset
		Given the node map
		 | a | b |

		Given the ways
		 | nodes |
		
		When I route I should get
		 | from | to | route |
		 | a    | b  |       |

	Scenario: Start/end point at the same location
		Given the node map
		 | a | b |
		 | 1 | 2 |

		Given the ways
		 | nodes |
		 | ab    |

		When I route I should get
		 | from | to | route |
		 | a    | a  |       |
		 | b    | b  |       |
		 | 1    | 1  |       |
		 | 2    | 2  |       |

	@poles
	Scenario: No routing close to the north/south pole
	Mercator is undefined close to the poles.
	All nodes and request with latitude to close to either of the poles should therefore be ignored.

		Given the node locations
		 | node | lat | lon |
		 | a    | 89  | 0   |
		 | b    | 87  | 0   |
		 | c    | 82  | 0   |
		 | d    | 80  | 0   |
		 | e    | 78  | 0   |
		 | k    | -78 | 0   |
		 | l    | -80 | 0   |
		 | m    | -82 | 0   |
		 | n    | -87 | 0   |
		 | o    | -89 | 0   |

		And the ways
		 | nodes |
		 | ab    |
		 | bc    |
		 | cd    |
		 | de    |
		 | kl    |
		 | lm    |
		 | mn    |
		 | no    |

		When I route I should get
		 | from | to | route |
		 | a    | b  |       |
		 | b    | c  |       |
		 | a    | d  |       |
		 | c    | d  | cd    |
		 | l    | m  | lm    |
		 | o    | l  |       |
		 | n    | m  |       |
		 | o    | n  |       |
