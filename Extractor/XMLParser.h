/*
    open source routing machine
    Copyright (C) Dennis Luxen, others 2010

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU AFFERO General Public License as published by
the Free Software Foundation; either version 3 of the License, or
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
or see http://www.gnu.org/licenses/agpl.txt.
 */

#ifndef XMLPARSER_H_
#define XMLPARSER_H_

#include <boost/ref.hpp>
#include <libxml/xmlreader.h>

#include "../typedefs.h"
#include "BaseParser.h"
#include "ExtractorStructs.h"
#include "../DataStructures/HashTable.h"
#include "../DataStructures/InputReaderFactory.h"

class XMLParser : public BaseParser<_Node, _RawRestrictionContainer, _Way> {
public:
    XMLParser(const char * filename) : nodeCallback(NULL), wayCallback(NULL), restrictionCallback(NULL), myLuaState(NULL){
        WARN("Parsing plain .osm/.osm.bz2 is deprecated. Switch to .pbf");
        inputReader = inputReaderFactory(filename);
    }
    virtual ~XMLParser() {}

    bool RegisterCallbacks(bool (*nodeCallbackPointer)(_Node), bool (*restrictionCallbackPointer)(_RawRestrictionContainer), bool (*wayCallbackPointer)(_Way)) {
        nodeCallback = *nodeCallbackPointer;
        wayCallback = *wayCallbackPointer;
        restrictionCallback = *restrictionCallbackPointer;
        return true;
    }

    void RegisterLUAState(lua_State *ml) {
        myLuaState = ml;
    }

    bool Init() {
        return (xmlTextReaderRead( inputReader ) == 1);
    }
    bool Parse() {
        if(0 != luaL_dostring( myLuaState, "return use_restrictions\n")) {
            ERR(lua_tostring(myLuaState,-1)<< " occured in scripting block");
        }
        bool useRestrictions = lua_toboolean(myLuaState, -1) != 0;

        while ( xmlTextReaderRead( inputReader ) == 1 ) {
            const int type = xmlTextReaderNodeType( inputReader );

            //1 is Element
            if ( type != 1 )
                continue;

            xmlChar* currentName = xmlTextReaderName( inputReader );
            if ( currentName == NULL )
                continue;

            if ( xmlStrEqual( currentName, ( const xmlChar* ) "node" ) == 1 ) {
                ImportNode n = _ReadXMLNode(  );
                /** Pass the unpacked node to the LUA call back **/
                try {
                    luabind::call_function<int>(
                            myLuaState,
                            "node_function",
                            boost::ref(n)
                    );
                    if(!(*nodeCallback)(n))
                        std::cerr << "[XMLParser] dense node not parsed" << std::endl;
                } catch (const luabind::error &er) {
                    cerr << er.what() << endl;
                    lua_State* Ler=er.state();
                    report_errors(Ler, -1);
                } catch (std::exception & e) {
                    ERR(e.what());
                } catch (...) {
                    ERR("Unknown error occurred during XML node parsing!");
                }
            }

            if ( xmlStrEqual( currentName, ( const xmlChar* ) "way" ) == 1 ) {
                string name;
                _Way way = _ReadXMLWay( );

                /** Pass the unpacked way to the LUA call back **/
                try {
                    luabind::call_function<int>(
                            myLuaState,
                            "way_function",
                            boost::ref(way),
                            way.path.size()
                    );
                    if(!(*wayCallback)(way)) {
                        std::cerr << "[PBFParser] way not parsed" << std::endl;
                    }
                } catch (const luabind::error &er) {
                    cerr << er.what() << endl;
                    lua_State* Ler=er.state();
                    report_errors(Ler, -1);
                } catch (std::exception & e) {
                    ERR(e.what());
                } catch (...) {
                    ERR("Unknown error occurred during XML way parsing!");
                }
            }
            if ( xmlStrEqual( currentName, ( const xmlChar* ) "relation" ) == 1 ) {
                _RawRestrictionContainer r = _ReadXMLRestriction();
                if(useRestrictions && r.fromWay != UINT_MAX) {
                    if(!(*restrictionCallback)(r)) {
                        std::cerr << "[XMLParser] restriction not parsed" << std::endl;
                    }
                }
            }
            xmlFree( currentName );
        }
        return true;
    }

private:
    _RawRestrictionContainer _ReadXMLRestriction ( ) {
        _RawRestrictionContainer restriction;

        if ( xmlTextReaderIsEmptyElement( inputReader ) != 1 ) {
            const int depth = xmlTextReaderDepth( inputReader );while ( xmlTextReaderRead( inputReader ) == 1 ) {
                const int childType = xmlTextReaderNodeType( inputReader );
                if ( childType != 1 && childType != 15 )
                    continue;
                const int childDepth = xmlTextReaderDepth( inputReader );
                xmlChar* childName = xmlTextReaderName( inputReader );
                if ( childName == NULL )
                    continue;

                if ( depth == childDepth && childType == 15 && xmlStrEqual( childName, ( const xmlChar* ) "relation" ) == 1 ) {
                    xmlFree( childName );
                    break;
                }
                if ( childType != 1 ) {
                    xmlFree( childName );
                    continue;
                }

                if ( xmlStrEqual( childName, ( const xmlChar* ) "tag" ) == 1 ) {
                    xmlChar* k = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "k" );
                    xmlChar* value = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "v" );
                    if ( k != NULL && value != NULL ) {
                        if(xmlStrEqual(k, ( const xmlChar* ) "restriction" )){
                            if(0 == std::string((const char *) value).find("only_"))
                                restriction.restriction.flags.isOnly = true;
                        }

                    }
                    if ( k != NULL )
                        xmlFree( k );
                    if ( value != NULL )
                        xmlFree( value );
                } else if ( xmlStrEqual( childName, ( const xmlChar* ) "member" ) == 1 ) {
                    xmlChar* ref = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "ref" );
                    if ( ref != NULL ) {
                        xmlChar * role = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "role" );
                        xmlChar * type = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "type" );
                        if(xmlStrEqual(role, (const xmlChar *) "to") && xmlStrEqual(type, (const xmlChar *) "way")) {
                            restriction.toWay = atoi((const char*) ref);
                        }
                        if(xmlStrEqual(role, (const xmlChar *) "from") && xmlStrEqual(type, (const xmlChar *) "way")) {
                            restriction.fromWay = atoi((const char*) ref);
                        }
                        if(xmlStrEqual(role, (const xmlChar *) "via") && xmlStrEqual(type, (const xmlChar *) "node")) {
                            restriction.restriction.viaNode = atoi((const char*) ref);
                        }

                        if(NULL != type)
                            xmlFree( type );
                        if(NULL != role)
                            xmlFree( role );
                        if(NULL != ref)
                            xmlFree( ref );
                    }
                }
                xmlFree( childName );
            }
        }
        return restriction;
    }

    _Way _ReadXMLWay( ) {
        _Way way;
        if ( xmlTextReaderIsEmptyElement( inputReader ) != 1 ) {
            const int depth = xmlTextReaderDepth( inputReader );
            while ( xmlTextReaderRead( inputReader ) == 1 ) {
                const int childType = xmlTextReaderNodeType( inputReader );
                if ( childType != 1 && childType != 15 )
                    continue;
                const int childDepth = xmlTextReaderDepth( inputReader );
                xmlChar* childName = xmlTextReaderName( inputReader );
                if ( childName == NULL )
                    continue;

                if ( depth == childDepth && childType == 15 && xmlStrEqual( childName, ( const xmlChar* ) "way" ) == 1 ) {
                    xmlChar* id = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "id" );
                    way.id = atoi((char*)id);
                    xmlFree(id);
                    xmlFree( childName );
                    break;
                }
                if ( childType != 1 ) {
                    xmlFree( childName );
                    continue;
                }

                if ( xmlStrEqual( childName, ( const xmlChar* ) "tag" ) == 1 ) {
                    xmlChar* k = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "k" );
                    xmlChar* value = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "v" );
                    //                cout << "->k=" << k << ", v=" << value << endl;
                    if ( k != NULL && value != NULL ) {

                        way.keyVals.Add(std::string( (char *) k ), std::string( (char *) value));
                    }
                    if ( k != NULL )
                        xmlFree( k );
                    if ( value != NULL )
                        xmlFree( value );
                } else if ( xmlStrEqual( childName, ( const xmlChar* ) "nd" ) == 1 ) {
                    xmlChar* ref = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "ref" );
                    if ( ref != NULL ) {
                        way.path.push_back( atoi(( const char* ) ref ) );
                        xmlFree( ref );
                    }
                }
                xmlFree( childName );
            }
        }
        return way;
    }

    ImportNode _ReadXMLNode( ) {
        ImportNode node;

        xmlChar* attribute = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "lat" );
        if ( attribute != NULL ) {
            node.lat =  static_cast<NodeID>(100000.*atof(( const char* ) attribute ) );
            xmlFree( attribute );
        }
        attribute = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "lon" );
        if ( attribute != NULL ) {
            node.lon =  static_cast<NodeID>(100000.*atof(( const char* ) attribute ));
            xmlFree( attribute );
        }
        attribute = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "id" );
        if ( attribute != NULL ) {
            node.id =  atoi(( const char* ) attribute );
            xmlFree( attribute );
        }

        if ( xmlTextReaderIsEmptyElement( inputReader ) != 1 ) {
            const int depth = xmlTextReaderDepth( inputReader );
            while ( xmlTextReaderRead( inputReader ) == 1 ) {
                const int childType = xmlTextReaderNodeType( inputReader );
                // 1 = Element, 15 = EndElement
                if ( childType != 1 && childType != 15 )
                    continue;
                const int childDepth = xmlTextReaderDepth( inputReader );
                xmlChar* childName = xmlTextReaderName( inputReader );
                if ( childName == NULL )
                    continue;

                if ( depth == childDepth && childType == 15 && xmlStrEqual( childName, ( const xmlChar* ) "node" ) == 1 ) {
                    xmlFree( childName );
                    break;
                }
                if ( childType != 1 ) {
                    xmlFree( childName );
                    continue;
                }

                if ( xmlStrEqual( childName, ( const xmlChar* ) "tag" ) == 1 ) {
                    xmlChar* k = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "k" );
                    xmlChar* value = xmlTextReaderGetAttribute( inputReader, ( const xmlChar* ) "v" );
                    if ( k != NULL && value != NULL ) {
                        node.keyVals.Add(std::string( reinterpret_cast<char*>(k) ), std::string( reinterpret_cast<char*>(value)));
                    }
                    if ( k != NULL )
                        xmlFree( k );
                    if ( value != NULL )
                        xmlFree( value );
                }

                xmlFree( childName );
            }
        }
        return node;
    }
    /* Input Reader */
    xmlTextReaderPtr inputReader;

    /* Function pointer for nodes */
    bool (*nodeCallback)(_Node);
    bool (*wayCallback)(_Way);
    bool (*restrictionCallback)(_RawRestrictionContainer);

    lua_State *myLuaState;
};

#endif /* XMLPARSER_H_ */
