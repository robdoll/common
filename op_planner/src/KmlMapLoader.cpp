/*
 * KmlMapLoader.cpp
 *
 *  Created on: Mar 16, 2020
 *      Author: hatem
 */

#include <op_planner/KmlMapLoader.h>
#include <op_planner/PlanningHelpers.h>

namespace PlannerHNS {

KmlMapLoader::KmlMapLoader(int map_version) : _map_version(map_version) {
}

KmlMapLoader::~KmlMapLoader() {
}

void KmlMapLoader::LoadKML(const std::string& kmlFile, RoadNetwork& map)
{
	//First, Get the main element
	TiXmlElement* pHeadElem = 0;
	TiXmlElement* pElem = 0;

	std::ifstream f(kmlFile.c_str());
	if(!f.good())
	{
		std::cout << "Can't Open KML Map File: (" << kmlFile << ")" << std::endl;
		return;
	}

	std::cout << " >> Loading KML Map file ... " <<  kmlFile << std::endl;

	TiXmlDocument doc(kmlFile);
	try
	{
		doc.LoadFile();
	}
	catch(std::exception& e)
	{
		std::cout << "KML Custom Reader Error, Can't Load .kml File, path is: "<<  kmlFile << std::endl;
		std::cout << e.what() << std::endl;
		return;
	}


	std::cout << " >> Reading Data from KML map file ... " << std::endl;

	pElem = doc.FirstChildElement();
	TiXmlElement* pOriginalType = GetDataFolder("OriginalFormat", pElem);
	if(pOriginalType != nullptr)
	{
		int iType = atoi(pOriginalType->GetText());
		switch(iType)
		{
		case 0:
		{
			map.original_map_format = MAP_VECTOR;
		}
		break;
		case 1:
		{
			map.original_map_format = MAP_KML;
		}
		break;
		case 2:
		{
			map.original_map_format = MAP_OPEN_DRIVE;
		}
		break;
		case 3:
		{
			map.original_map_format = MAP_LANELET2;
		}
		break;
		default:
		{
			map.original_map_format = MAP_KML;
		}
		break;
		}
	}

	TiXmlElement* pProjStr= GetDataFolder("ProjectionString", pElem);
	if(pProjStr != nullptr)
	{
		map.str_proj = pProjStr->GetText();
	}

	TiXmlElement* pOrigin = GetDataFolder("Origin", pElem);
	if(pOrigin != nullptr)
	{
		map.origin = GetWaypointsData(pOrigin).at(0);
	}
	pHeadElem = GetHeadElement(pElem);

	std::cout << " >> Load Lanes from KML file .. " << std::endl;
	std::vector<Lane> laneLinksList = GetLanesList(pHeadElem);

	map.roadSegments.clear();
	map.roadSegments = GetRoadSegmentsList(pHeadElem);

	std::cout << " >> Load Traffic lights from KML file .. " << std::endl;
	std::vector<TrafficLight> trafficLights = GetTrafficLightsList(pHeadElem);

	std::cout << " >> Load Stop lines from KML file .. " << std::endl;
	std::vector<StopLine> stopLines = GetStopLinesList(pHeadElem);

	std::cout << " >> Load Signes from KML file .. " << std::endl;
	std::vector<TrafficSign> signs = GetTrafficSignsList(pHeadElem);

	std::cout << " >> Load Crossings from KML file .. " << std::endl;
	std::vector<Crossing> crossings = GetCrossingsList(pHeadElem);

	std::cout << " >> Load Markings from KML file .. " << std::endl;
	std::vector<Marking> markings = GetMarkingsList(pHeadElem);

	std::cout << " >> Load Road boundaries from KML file .. " << std::endl;
	std::vector<Boundary> boundaries = GetBoundariesList(pHeadElem);

	std::cout << " >> Load Curbs from KML file .. " << std::endl;
	std::vector<Curb> curbs = GetCurbsList(pHeadElem);

	std::cout << " >> Load Lines from KML file .. " << std::endl;
	std::vector<Line> lines = GetLinesList(pHeadElem);

	map.signs.clear();
	map.signs = signs;

	map.crossings.clear();
	map.crossings = crossings;

	map.markings.clear();
	map.markings = markings;

	map.boundaries.clear();
	map.boundaries = boundaries;

	map.curbs.clear();
	map.curbs = curbs;

	map.lines.clear();
	map.lines = lines;

	//Fill the relations
	for(unsigned int i= 0; i<map.roadSegments.size(); i++ )
	{
		for(unsigned int j=0; j < laneLinksList.size(); j++)
		{
			PlanningHelpers::CalcAngleAndCost(laneLinksList.at(j).points);
			map.roadSegments.at(i).Lanes.push_back(laneLinksList.at(j));
		}
	}

	std::cout << " >> Link lanes and waypoints with pointers ... " << std::endl;
	//Link Lanes and lane's waypoints by pointers
	for(unsigned int rs = 0; rs < map.roadSegments.size(); rs++)
	{
		//Link Lanes
		for(unsigned int i =0; i < map.roadSegments.at(rs).Lanes.size(); i++)
		{
			Lane* pL = &map.roadSegments.at(rs).Lanes.at(i);
			for(unsigned int j = 0 ; j < pL->fromIds.size(); j++)
			{
				for(unsigned int l= 0; l < map.roadSegments.at(rs).Lanes.size(); l++)
				{
					if(map.roadSegments.at(rs).Lanes.at(l).id == pL->fromIds.at(j))
					{
						pL->fromLanes.push_back(&map.roadSegments.at(rs).Lanes.at(l));
					}
				}
			}

			for(unsigned int j = 0 ; j < pL->toIds.size(); j++)
			{
				for(unsigned int l= 0; l < map.roadSegments.at(rs).Lanes.size(); l++)
				{
					if(map.roadSegments.at(rs).Lanes.at(l).id == pL->toIds.at(j))
					{
						pL->toLanes.push_back(&map.roadSegments.at(rs).Lanes.at(l));
					}
				}
			}

			for(unsigned int j = 0 ; j < pL->points.size(); j++)
			{
				pL->points.at(j).pLane  = pL;
			}
		}
	}

	//Link waypoints
	std::cout << " >> Link missing branches and waypoints... " << std::endl;
	MappingHelpers::LinkMissingBranchingWayPointsV2(map);

	std::cout << " >> Link Lane change semantics ... " << std::endl;
	for(unsigned int rs = 0; rs < map.roadSegments.size(); rs++)
	{
		for(unsigned int i =0; i < map.roadSegments.at(rs).Lanes.size(); i++)
		{
			for(unsigned int p= 0; p < map.roadSegments.at(rs).Lanes.at(i).points.size(); p++)
			{
				WayPoint* pWP = &map.roadSegments.at(rs).Lanes.at(i).points.at(p);
				if(pWP->pLeft == 0 && pWP->LeftPointId > 0)
				{
					pWP->pLeft = MappingHelpers::FindWaypointV2(pWP->LeftPointId, pWP->laneId, map);

					if(pWP->pLeft != nullptr)
					{
						pWP->LeftLnId = pWP->pLeft->laneId;
						pWP->pLane->pLeftLane = pWP->pLeft->pLane;

						if(pWP->pLeft->RightPointId == pWP->id)
						{
							pWP->pLeft->pRight = pWP;
							pWP->pLeft->RightLnId = pWP->laneId;
							pWP->pLeft->pLane->pRightLane = pWP->pLane;
						}
					}
				}

				if(pWP->pRight == 0 && pWP->RightPointId > 0)
				{
					pWP->pRight = MappingHelpers::FindWaypointV2(pWP->RightPointId, pWP->laneId, map);

					if(pWP->pRight != nullptr)
					{
						pWP->RightLnId = pWP->pRight->laneId;
						pWP->pLane->pRightLane = pWP->pRight->pLane;

						if(pWP->pRight->LeftPointId == pWP->id)
						{
							pWP->pRight->pLeft = pWP;
							pWP->pRight->LeftLnId = pWP->laneId;
							pWP->pRight->pLane->pLeftLane = pWP->pLane;
						}
					}
				}
			}
		}
	}

	map.stopLines = stopLines;
	map.trafficLights = trafficLights;

	//Link waypoints && StopLines
	std::cout << " >> Link Stop lines and Traffic lights ... " << std::endl;
	for(unsigned int rs = 0; rs < map.roadSegments.size(); rs++)
	{
		for(unsigned int i =0; i < map.roadSegments.at(rs).Lanes.size(); i++)
		{
			for(unsigned int itl = 0; itl < map.trafficLights.size(); itl++)
			{
				if(map.trafficLights.at(itl).CheckLane(map.roadSegments.at(rs).Lanes.at(i).id))
				{
					map.trafficLights.at(itl).pLanes.push_back(&map.roadSegments.at(rs).Lanes.at(i));
					map.roadSegments.at(rs).Lanes.at(i).trafficlights.push_back(map.trafficLights.at(itl));
				}
			}

			for(unsigned int isl = 0; isl < map.stopLines.size(); isl++)
			{
				if(map.stopLines.at(isl).laneId == map.roadSegments.at(rs).Lanes.at(i).id)
				{
					map.stopLines.at(isl).pLane = &map.roadSegments.at(rs).Lanes.at(i);
					map.roadSegments.at(rs).Lanes.at(i).stopLines.push_back(map.stopLines.at(isl));
					WayPoint wp((map.stopLines.at(isl).points.at(0).pos.x+map.stopLines.at(isl).points.at(1).pos.x)/2.0, (map.stopLines.at(isl).points.at(0).pos.y+map.stopLines.at(isl).points.at(1).pos.y)/2.0, (map.stopLines.at(isl).points.at(0).pos.z+map.stopLines.at(isl).points.at(1).pos.z)/2.0, (map.stopLines.at(isl).points.at(0).pos.a+map.stopLines.at(isl).points.at(1).pos.a)/2.0);
					map.roadSegments.at(rs).Lanes.at(i).points.at(PlanningHelpers::GetClosestNextPointIndexFast(map.roadSegments.at(rs).Lanes.at(i).points, wp)).stopLineID = map.stopLines.at(isl).id;
				}
			}
		}
	}

	//Link waypoints && StopLines
	std::cout << " >> Link Boundaries and Waypoints ... " << std::endl;
	MappingHelpers::ConnectBoundariesToWayPoints(map);
	MappingHelpers::LinkBoundariesToWayPoints(map);
	MappingHelpers::LinkTrafficLightsIntoGroups(map);
	MappingHelpers::ConnectTrafficLightsAndStopLines(map);
	MappingHelpers::ConnectTrafficSignsAndStopLines(map);

	std::cout << " >> Find Max IDs ... " << std::endl;
	MappingHelpers::GetMapMaxIds(map);

	std::cout << "Map loaded from kml file with (" << laneLinksList.size()  << ") lanes, First Point ( " << MappingHelpers::GetFirstWaypoint(map).pos.ToString() << ")"<< std::endl;
}

TiXmlElement* KmlMapLoader::GetHeadElement(TiXmlElement* pMainElem)
{
	TiXmlElement* pElem = pMainElem;
	if(pElem)
		pElem = pElem->FirstChildElement("Folder");
	if(pElem && pElem->FirstChildElement("Folder"))
		pElem = pElem->FirstChildElement("Folder");
	if(pElem && pElem->FirstChildElement("Document"))
		pElem = pElem->FirstChildElement("Document");

	if(!pElem)
	{
		return nullptr;
	}
	return pElem;
}

TiXmlElement* KmlMapLoader::GetDataFolder(const std::string& folderName, TiXmlElement* pMainElem)
{
	if(!pMainElem) return nullptr;

	TiXmlElement* pElem = pMainElem->FirstChildElement("Folder");

	std::string folderID = "";
	for(; pElem; pElem=pElem->NextSiblingElement())
	{
		folderID="";
		if(pElem->FirstChildElement("name")->GetText()) //Map Name
			folderID = pElem->FirstChildElement("name")->GetText();
		if(folderID.compare(folderName)==0)
			return pElem;
	}
	return nullptr;
}

std::vector<Curb> KmlMapLoader::GetCurbsList(TiXmlElement* pElem)
{
	std::vector<Curb> cList;
	TiXmlElement* pCurbs = GetDataFolder("CurbsLines", pElem);
	if(!pCurbs)
		return cList;

	TiXmlElement* pE = pCurbs->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml = pE->FirstChildElement("name");

		if(pNameXml)
		{
			tfID = pNameXml->GetText();
			Curb c;
			c.id = GetIDsFromPrefix(tfID, "BID", "LnID").at(0);
			c.laneId = GetIDsFromPrefix(tfID, "LnID", "RdID").at(0);
			c.roadId = GetIDsFromPrefix(tfID, "RdID", "WID").at(0);
			c.width = GetDoubleFromPrefix(tfID, "WID", "HEI").at(0);
			c.height = GetDoubleFromPrefix(tfID, "HEI", "").at(0);

			TiXmlElement* pPoints = pE->FirstChildElement("LineString")->FirstChildElement("coordinates");
			c.points = GetWaypointsData(pPoints);

			cList.push_back(c);
		}
	}

	return cList;
}

std::vector<Line> KmlMapLoader::GetLinesList(TiXmlElement* pElem)
{
	std::vector<Line> l_list;
	TiXmlElement* pLines = GetDataFolder("Lines", pElem);
	if(!pLines)
		return l_list;

	TiXmlElement* pE = pLines->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		TiXmlElement* pNameXml = pE->FirstChildElement("description");


		if(pNameXml)
		{
			std::string line_ID = pNameXml->GetText();
			Line l;
			l.id = GetIDsFromPrefix(line_ID, "LIID", "RdID").at(0);
			l.roadID = GetIDsFromPrefix(line_ID, "RdID", "Wid").at(0);
			l.width = GetDoubleFromPrefix(line_ID, "Wid", "Typ").at(0);
			l.type = MappingHelpers::FromTextToLineType(GetStringsFromPrefix(line_ID, "Typ", "Clr").at(0));
			l.color = MappingHelpers::FromTextToMarkColor(GetStringsFromPrefix(line_ID, "Clr", "LLn").at(0));
			l.left_lane_ids = GetIDsFromPrefix(line_ID, "LLn", "RLn");
			l.right_lane_ids = GetIDsFromPrefix(line_ID, "RLn", "");

			TiXmlElement* pPoints = pE->FirstChildElement("LineString")->FirstChildElement("coordinates");
			l.points = GetWaypointsData(pPoints);

			l_list.push_back(l);
		}
	}

	return l_list;
}

std::vector<Boundary> KmlMapLoader::GetBoundariesList(TiXmlElement* pElem)
{
	std::vector<Boundary> bList;
	TiXmlElement* pBoundaries = GetDataFolder("Boundaries", pElem);
	if(!pBoundaries)
		return bList;

	TiXmlElement* pE = pBoundaries->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml = pE->FirstChildElement("name");

		if(pNameXml)
		{
			tfID = pNameXml->GetText();
			Boundary b;
			b.id = GetIDsFromPrefix(tfID, "BID", "RdID").at(0);
			b.roadId = GetIDsFromPrefix(tfID, "RdID", "").at(0);

			TiXmlElement* pPoints = pE->FirstChildElement("LineString")->FirstChildElement("coordinates");
			b.points = GetWaypointsData(pPoints);

			bList.push_back(b);
		}
	}

	return bList;
}

std::vector<Marking> KmlMapLoader::GetMarkingsList(TiXmlElement* pElem)
{
	std::vector<Marking> mList;
	TiXmlElement* pMarkings= GetDataFolder("Markings", pElem);
	if(!pMarkings)
		return mList;

	TiXmlElement* pE = pMarkings->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml =pE->FirstChildElement("name");

		if(pNameXml)
		{
			tfID = pNameXml->GetText();
			Marking m;
			m.id = GetIDsFromPrefix(tfID, "MID", "LnID").at(0);
			m.laneId = GetIDsFromPrefix(tfID, "LnID", "RdID").at(0);
			m.roadId = GetIDsFromPrefix(tfID, "RdID", "").at(0);

			TiXmlElement* pPoints = pE->FirstChildElement("LineString")->FirstChildElement("coordinates");

			m.points = GetWaypointsData(pPoints);

			if(m.points.size() > 0)
			{
				//first item is the center of the marking
				m.center = m.points.at(0);
				m.points.erase(m.points.begin()+0);
			}

			mList.push_back(m);
		}
	}

	return mList;
}

std::vector<Crossing> KmlMapLoader::GetCrossingsList(TiXmlElement* pElem)
{
	std::vector<Crossing> crossList;
	TiXmlElement* pCrossings= GetDataFolder("Crossings", pElem);

	if(!pCrossings)
		return crossList;

	TiXmlElement* pE = pCrossings->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml =pE->FirstChildElement("name");

		if(pNameXml)
		{
			tfID = pNameXml->GetText();
			Crossing cross;
			cross.id = GetIDsFromPrefix(tfID, "CRID", "RdID").at(0);
			cross.roadId = GetIDsFromPrefix(tfID, "RdID", "").at(0);

			TiXmlElement* pPoints = pE->FirstChildElement("LineString")->FirstChildElement("coordinates");
			cross.points = GetWaypointsData(pPoints);

			crossList.push_back(cross);
		}
	}

	return crossList;
}

std::vector<TrafficSign> KmlMapLoader::GetTrafficSignsList(TiXmlElement* pElem)
{
	std::vector<TrafficSign> tsList;
	TiXmlElement* pSigns = GetDataFolder("TrafficSigns", pElem);

	if(!pSigns)
		return tsList;

	TiXmlElement* pE = pSigns->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml =pE->FirstChildElement("name");

		if(pNameXml)
		{
		  tfID = pNameXml->GetText();

		  	TrafficSign ts;
			ts.id = GetIDsFromPrefix(tfID, "TSID", "LnID").at(0);
			ts.laneIds = GetIDsFromPrefix(tfID, "LnID", "GpID");
			ts.groupID = GetIDsFromPrefix(tfID, "GpID", "Type").at(0);
			int iType = GetIDsFromPrefix(tfID, "Type", "VANG").at(0);
			switch(iType)
			{
			case 0:
				ts.signType = UNKNOWN_SIGN;
				break;
			case 1:
				ts.signType = STOP_SIGN;
				break;
			case 2:
				ts.signType = MAX_SPEED_SIGN;
				break;
			case 3:
				ts.signType = MIN_SPEED_SIGN;
				break;
			default:
				ts.signType = STOP_SIGN;
				break;
			}

			ts.vertical_angle = GetDoubleFromPrefix(tfID, "VANG", "HANG").at(0);
			ts.horizontal_angle = GetDoubleFromPrefix(tfID, "HANG", "").at(0);

			TiXmlElement* pPoints = pE->FirstChildElement("Point")->FirstChildElement("coordinates");
			ts.pose = GetWaypointsData(pPoints).at(0);

			tsList.push_back(ts);
		}
	}

	return tsList;
}

std::vector<StopLine> KmlMapLoader::GetStopLinesList(TiXmlElement* pElem)
{
	std::vector<StopLine> slList;
	TiXmlElement* pStopLines = GetDataFolder("StopLines", pElem);

	if(!pStopLines)
		return slList;

	TiXmlElement* pE = pStopLines->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml =pE->FirstChildElement("name");

		if(pNameXml)
		{
		  tfID = pNameXml->GetText();

			StopLine sl;
			sl.id = GetIDsFromPrefix(tfID, "SLID", "LnID").at(0);
			sl.laneId = GetIDsFromPrefix(tfID, "LnID", "TSID").at(0);
			sl.stopSignID = GetIDsFromPrefix(tfID, "TSID", "TLID").at(0);
			sl.lightIds = GetIDsFromPrefix(tfID, "TLID", "");


			TiXmlElement* pPoints = pE->FirstChildElement("LineString")->FirstChildElement("coordinates");
			sl.points = GetWaypointsData(pPoints);

			slList.push_back(sl);
		}
	}

	return slList;
}

std::vector<TrafficLight> KmlMapLoader::GetTrafficLightsList(TiXmlElement* pElem)
{
	std::vector<TrafficLight> tlList;
	TiXmlElement* pLightsLines = GetDataFolder("TrafficLights", pElem);

	if(!pLightsLines)
		return tlList;

	TiXmlElement* pE = pLightsLines->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml =pE->FirstChildElement("name");

		if(pNameXml)
		{
		  tfID = pNameXml->GetText();

			TrafficLight tl;
			tl.id = GetIDsFromPrefix(tfID, "TLID", "LnID").at(0);
			tl.laneIds = GetIDsFromPrefix(tfID, "LnID", "GpID");
			tl.groupID = GetIDsFromPrefix(tfID, "GpID", "Type").at(0);
			int iType = GetIDsFromPrefix(tfID, "Type", "VANG").at(0);
			switch(iType)
			{
			case 1:
				tl.lightType = RED_LIGHT;
				break;
			case 2:
				tl.lightType = GREEN_LIGHT;
				break;
			case 3:
				tl.lightType = YELLOW_LIGHT;
				break;
			case 4:
				tl.lightType = CROSS_GREEN;
				break;
			case 5:
				tl.lightType = CROSS_RED;
				break;
			case 6:
				tl.lightType = LEFT_GREEN;
				break;
			case 7:
				tl.lightType = FORWARD_GREEN;
				break;
			case 8:
				tl.lightType = RIGHT_GREEN;
				break;
			case 9:
				tl.lightType = FLASH_YELLOW;
				break;
			case 10:
				tl.lightType = FLASH_RED;
				break;
			default:
				tl.lightType = UNKNOWN_LIGHT;
				break;
			}
			tl.vertical_angle = GetDoubleFromPrefix(tfID, "VANG", "HANG").at(0);
			tl.horizontal_angle = GetDoubleFromPrefix(tfID, "HANG", "").at(0);


			TiXmlElement* pPoints = pE->FirstChildElement("Point")->FirstChildElement("coordinates");
			tl.pose = GetWaypointsData(pPoints).at(0);

			tlList.push_back(tl);
		}
	}

	return tlList;
}

std::vector<Lane> KmlMapLoader::GetLanesList(TiXmlElement* pElem)
{
	std::vector<Lane> llList;
	TiXmlElement* pLaneLinks = GetDataFolder("Lanes", pElem);

	if(!pLaneLinks)
		return llList;

	TiXmlElement* pE = pLaneLinks->FirstChildElement("Folder");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml =pE->FirstChildElement("description");
		if(pNameXml)
		{
		  tfID = pNameXml->GetText();

			Lane ll;
			ll.id = GetIDsFromPrefix(tfID, "LID", "RSID").at(0);
			ll.roadId = GetIDsFromPrefix(tfID, "RSID", "NUM").at(0);
			ll.num = GetIDsFromPrefix(tfID, "NUM", "From").at(0);
			ll.fromIds = GetIDsFromPrefix(tfID, "From", "To");
			ll.toIds = GetIDsFromPrefix(tfID, "To", "Vel");
			ll.speed = GetDoubleFromPrefix(tfID, "Vel", "").at(0);
	if(_map_version == 0)
			ll.points = GetCenterLaneDataVer0(pE, ll.id);
	else
			ll.points = GetCenterLaneData(pE, ll.id);

			llList.push_back(ll);
		}
	}

	return llList;
}

std::vector<RoadSegment> KmlMapLoader::GetRoadSegmentsList(TiXmlElement* pElem)
{
	std::vector<RoadSegment> rlList;
	TiXmlElement* pRoadLinks = GetDataFolder("RoadSegments", pElem);

	if(!pRoadLinks)
		return rlList;

	TiXmlElement* pE = pRoadLinks->FirstChildElement("Placemark");
	for( ; pE; pE=pE->NextSiblingElement())
	{
		std::string tfID;
		TiXmlElement* pNameXml =pE->FirstChildElement("description");
		if(pNameXml)
		  tfID = pNameXml->GetText();

		RoadSegment rl;
		rl.id = GetIDsFromPrefix(tfID, "RSID", "").at(0);
		rlList.push_back(rl);
	}

	return rlList;
}

std::vector<WayPoint> KmlMapLoader::GetWaypointsData(TiXmlElement* pElem)
{
	std::vector<WayPoint> points;
	if(pElem)
	{
		std::string coordinate_list;
		if(!pElem->NoChildren())
			coordinate_list = pElem->GetText();

		std::istringstream str_stream(coordinate_list);
		std::string token, temp;

		while(getline(str_stream, token, ' '))
		{
			std::string lat, lon, alt;
			double numLat=0, numLon=0, numAlt=0;

			std::istringstream ss(token);

			getline(ss, lat, ',');
			getline(ss, lon, ',');
			getline(ss, alt, ',');

			numLat = atof(lat.c_str());
			numLon = atof(lon.c_str());
			numAlt = atof(alt.c_str());

			WayPoint p;

			p.pos.x = p.pos.lat = numLat;
			p.pos.y = p.pos.lon = numLon;
			p.pos.z = p.pos.alt = numAlt;
			points.push_back(p);
		}
	}

	return points;
}

std::vector<WayPoint> KmlMapLoader::GetCenterLaneData(TiXmlElement* pElem, const int& currLaneID)
{
	std::vector<WayPoint> gps_points;

	TiXmlElement* pV = pElem->FirstChildElement("Placemark");

	if(pV)
	 pV = pV->FirstChildElement("LineString");

	if(pV)
		pV = pV->FirstChildElement("coordinates");

	if(pV)
	{
		std::string coordinate_list;
		if(!pV->NoChildren())
			coordinate_list = pV->GetText();

		std::istringstream str_stream(coordinate_list);
		std::string token, temp;


		while(getline(str_stream, token, ' '))
		{
			std::string lat, lon, alt;
			double numLat=0, numLon=0, numAlt=0;

			std::istringstream ss(token);

			getline(ss, lat, ',');
			getline(ss, lon, ',');
			getline(ss, alt, ',');

			numLat = atof(lat.c_str());
			numLon = atof(lon.c_str());
			numAlt = atof(alt.c_str());

			WayPoint wp;

			wp.pos.x = wp.pos.lat = numLat;
			wp.pos.y = wp.pos.lon = numLon;
			wp.pos.z = wp.pos.alt = numAlt;

			wp.laneId = currLaneID;
			gps_points.push_back(wp);
		}

		TiXmlElement* pInfoEl =pElem->FirstChildElement("Folder")->FirstChildElement("description");
		std::string additional_info;
		if(pInfoEl)
			additional_info = pInfoEl->GetText();
		additional_info.insert(additional_info.begin(), ',');
		additional_info.erase(additional_info.end()-1);
		std::vector<std::string> add_info_list = SplitString(additional_info, ",");
		if(gps_points.size() == add_info_list.size())
		{
			for(unsigned int i=0; i< gps_points.size(); i++)
			{
				gps_points.at(i).id =  GetIDsFromPrefix(add_info_list.at(i), "WPID", "AC").at(0);
				gps_points.at(i).actionCost.push_back(GetActionPairFromPrefix(add_info_list.at(i), "AC", "From"));
				gps_points.at(i).fromIds =  GetIDsFromPrefix(add_info_list.at(i), "From", "To");
				gps_points.at(i).toIds =  GetIDsFromPrefix(add_info_list.at(i), "To", "Lid");

				std::vector<int> ids = GetIDsFromPrefix(add_info_list.at(i), "Lid", "Rid");
				if(ids.size() > 0)
					gps_points.at(i).LeftPointId =  ids.at(0);

				ids = GetIDsFromPrefix(add_info_list.at(i), "Rid", "Vel");
				if(ids.size() > 0)
					gps_points.at(i).RightPointId =  ids.at(0);

				std::vector<double> dnums = GetDoubleFromPrefix(add_info_list.at(i), "Vel", "Dir");
				if(dnums.size() > 0)
					gps_points.at(i).v =  dnums.at(0);

				dnums = GetDoubleFromPrefix(add_info_list.at(i), "Dir", "");
				if(dnums.size() > 0)
					gps_points.at(i).pos.a = gps_points.at(i).pos.dir =  dnums.at(0);
			}
		}
	}

	return gps_points;
}

std::vector<WayPoint> KmlMapLoader::GetCenterLaneDataVer0(TiXmlElement* pElem, const int& currLaneID)
{
	std::vector<WayPoint> gps_points;

	TiXmlElement* pV = pElem->FirstChildElement("Placemark");

	if(pV)
	 pV = pV->FirstChildElement("LineString");

	if(pV)
		pV = pV->FirstChildElement("coordinates");

	if(pV)
	{
		std::string coordinate_list;
		if(!pV->NoChildren())
			coordinate_list = pV->GetText();

		std::istringstream str_stream(coordinate_list);
		std::string token, temp;


		while(getline(str_stream, token, ' '))
		{
			std::string lat, lon, alt;
			double numLat=0, numLon=0, numAlt=0;

			std::istringstream ss(token);

			getline(ss, lat, ',');
			getline(ss, lon, ',');
			getline(ss, alt, ',');

			numLat = atof(lat.c_str());
			numLon = atof(lon.c_str());
			numAlt = atof(alt.c_str());

			WayPoint wp;

			wp.pos.x = wp.pos.lat = numLat;
			wp.pos.y = wp.pos.lon = numLon;
			wp.pos.z = wp.pos.alt = numAlt;

			wp.laneId = currLaneID;
			gps_points.push_back(wp);
		}

		TiXmlElement* pInfoEl =pElem->FirstChildElement("Folder")->FirstChildElement("description");
		std::string additional_info;
		if(pInfoEl)
			additional_info = pInfoEl->GetText();
		additional_info.insert(additional_info.begin(), ',');
		additional_info.erase(additional_info.end()-1);
		std::vector<std::string> add_info_list = SplitString(additional_info, ",");
		if(gps_points.size() == add_info_list.size())
		{
			for(unsigned int i=0; i< gps_points.size(); i++)
			{
				gps_points.at(i).id =  GetIDsFromPrefix(add_info_list.at(i), "WPID", "C").at(0);
				gps_points.at(i).fromIds =  GetIDsFromPrefix(add_info_list.at(i), "From", "To");
				gps_points.at(i).toIds =  GetIDsFromPrefix(add_info_list.at(i), "To", "Vel");
				gps_points.at(i).v =  GetDoubleFromPrefix(add_info_list.at(i), "Vel", "Dir").at(0);
				gps_points.at(i).pos.a = gps_points.at(i).pos.dir =  GetDoubleFromPrefix(add_info_list.at(i), "Dir", "").at(0);
			}
		}
	}

	return gps_points;
}

std::vector<int> KmlMapLoader::GetIDsFromPrefix(const std::string& str, const std::string& prefix, const std::string& postfix)
{
	int index1 = str.find(prefix)+prefix.size();
	int index2 = str.find(postfix, index1);
	if(index2 < 0  || postfix.size() ==0)
		index2 = str.size();

	std::string str_ids = str.substr(index1, index2-index1);

	std::vector<int> ids;
	std::vector<std::string> idstr = SplitString(str_ids, "_");

	for(unsigned  int i=0; i< idstr.size(); i++ )
	{
		if(idstr.at(i).size()>0)
		{
			int num = atoi(idstr.at(i).c_str());
			//if(num>-1)
				ids.push_back(num);
		}
	}

	return ids;
}

std::vector<std::string> KmlMapLoader::GetStringsFromPrefix(const std::string& str, const std::string& prefix, const std::string& postfix)
{
	int index1 = str.find(prefix)+prefix.size();
	int index2 = str.find(postfix, index1);
	if(index2 < 0  || postfix.size() ==0)
		index2 = str.size();

	std::string str_ids = str.substr(index1, index2-index1);

	std::vector<std::string> ids;
	std::vector<std::string> idstr = SplitString(str_ids, "_");

	for(unsigned  int i=0; i< idstr.size(); i++ )
	{
		if(idstr.at(i).size()>0)
		{
			ids.push_back(idstr.at(i));
		}
	}

	return ids;
}

std::vector<double> KmlMapLoader::GetDoubleFromPrefix(const std::string& str, const std::string& prefix, const std::string& postfix)
{
	int index1 = str.find(prefix)+prefix.size();
	int index2 = str.find(postfix, index1);
	if(index2 < 0  || postfix.size() ==0)
		index2 = str.size();

	std::string str_ids = str.substr(index1, index2-index1);

	std::vector<double> ids;
	std::vector<std::string> idstr = SplitString(str_ids, "_");

	for(unsigned  int i=0; i< idstr.size(); i++ )
	{
		if(idstr.at(i).size()>0)
		{
			double num = atof(idstr.at(i).c_str());
			//if(num>-1)
				ids.push_back(num);
		}
	}

	return ids;
}

std::pair<ACTION_TYPE, double> KmlMapLoader::GetActionPairFromPrefix(const std::string& str, const std::string& prefix, const std::string& postfix)
{
	int index1 = str.find(prefix)+prefix.size();
	int index2 = str.find(postfix, index1);
	if(index2<0  || postfix.size() ==0)
		index2 = str.size();

	std::string str_ids = str.substr(index1, index2-index1);

	std::pair<ACTION_TYPE, double> act_cost;
	act_cost.first = FORWARD_ACTION;
	act_cost.second = 0;

	std::vector<std::string> idstr = SplitString(str_ids, "_");

	if(idstr.size() >= 2)
	{
		if(idstr.at(0).size() > 0 && idstr.at(0).at(0) == 'L')
			act_cost.first = LEFT_TURN_ACTION;
		else if(idstr.at(0).size() > 0 && idstr.at(0).at(0) == 'R')
			act_cost.first = RIGHT_TURN_ACTION;
		if(idstr.at(1).size() > 0)
			act_cost.second = atof(idstr.at(1).c_str());
	}

	return act_cost;
}

std::vector<std::string> KmlMapLoader::SplitString(const std::string& str, const std::string& token)
{
	std::vector<std::string> str_parts;
	int iFirstPart = str.find(token);

	while(iFirstPart >= 0)
	{
		iFirstPart++;
		int iSecondPart = str.find(token, iFirstPart);
		if(iSecondPart>0)
		{
			str_parts.push_back(str.substr(iFirstPart,iSecondPart - iFirstPart));
		}
		else
		{
			str_parts.push_back(str.substr(iFirstPart,str.size() - iFirstPart));
		}

		iFirstPart = iSecondPart;
	}

	return str_parts;
}

} /* namespace PlannerHNS */
