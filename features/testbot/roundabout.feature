@routing @testbot @roundabout @instruction
Feature: Roundabout Instructions

    Background:
        Given the profile "testbot"

    Scenario: Testbot - Roundabout
        Given the node map
            |   |   | v |   |   |
            |   |   | d |   |   |
            | s | a |   | c | u |
            |   |   | b |   |   |
            |   |   | t |   |   |

        And the ways
            | nodes | junction   |
            | sa    |            |
            | tb    |            |
            | uc    |            |
            | vd    |            |
            | abcda | roundabout |

        When I route I should get
            | from | to | route | turns                            |
            | s    | t  | sa,tb | depart,enter_roundabout-1,arrive |
            | s    | u  | sa,uc | depart,enter_roundabout-2,arrive |
            | s    | v  | sa,vd | depart,enter_roundabout-3,arrive |
            | t    | u  | tb,uc | depart,enter_roundabout-1,arrive |
            | t    | v  | tb,vd | depart,enter_roundabout-2,arrive |
            | t    | s  | tb,sa | depart,enter_roundabout-3,arrive |
            | u    | v  | uc,vd | depart,enter_roundabout-1,arrive |
            | u    | s  | uc,sa | depart,enter_roundabout-2,arrive |
            | u    | t  | uc,tb | depart,enter_roundabout-3,arrive |
            | v    | s  | vd,sa | depart,enter_roundabout-1,arrive |
            | v    | t  | vd,tb | depart,enter_roundabout-2,arrive |
            | v    | u  | vd,uc | depart,enter_roundabout-3,arrive |

    Scenario: Testbot - Roundabout with oneway links
        Given the node map
            |   |   | p | o |   |   |
            |   |   | h | g |   |   |
            | i | a |   |   | f | n |
            | j | b |   |   | e | m |
            |   |   | c | d |   |   |
            |   |   | k | l |   |   |

        And the ways
            | nodes     | junction   | oneway |
            | ai        |            | yes    |
            | jb        |            | yes    |
            | ck        |            | yes    |
            | ld        |            | yes    |
            | em        |            | yes    |
            | nf        |            | yes    |
            | go        |            | yes    |
            | ph        |            | yes    |
            | abcdefgha | roundabout |        |

        When I route I should get
            | from | to | route | turns                            |
            | j    | k  | jb,ck | depart,enter_roundabout-1,arrive |
            | j    | m  | jb,em | depart,enter_roundabout-2,arrive |
            | j    | o  | jb,go | depart,enter_roundabout-3,arrive |
            | j    | i  | jb,ai | depart,enter_roundabout-4,arrive |
            | l    | m  | ld,em | depart,enter_roundabout-1,arrive |
            | l    | o  | ld,go | depart,enter_roundabout-2,arrive |
            | l    | i  | ld,ai | depart,enter_roundabout-3,arrive |
            | l    | k  | ld,ck | depart,enter_roundabout-4,arrive |
            | n    | o  | nf,go | depart,enter_roundabout-1,arrive |
            | n    | i  | nf,ai | depart,enter_roundabout-2,arrive |
            | n    | k  | nf,ck | depart,enter_roundabout-3,arrive |
            | n    | m  | nf,em | depart,enter_roundabout-4,arrive |
            | p    | i  | ph,ai | depart,enter_roundabout-1,arrive |
            | p    | k  | ph,ck | depart,enter_roundabout-2,arrive |
            | p    | m  | ph,em | depart,enter_roundabout-3,arrive |
            | p    | o  | ph,go | depart,enter_roundabout-4,arrive |
