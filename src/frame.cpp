#include <internal/internal.h>
#include <imgui.h>
#include "internal/widgets.h"
#include <imgui_node_editor.h>
#include <internal/example_property_im_draw.h>


#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <internal/draw_utils.h>
#include <internal/draw_nodes.h>
#include <internal/handle_interactions.h>

using namespace plano::types;
using namespace plano::api;
using namespace plano::internal;
namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;
using namespace ax;
using ax::Widgets::IconType;


namespace plano {
namespace api {

void ShowLeftPane()
{
	auto& io = ImGui::GetIO();
	
	float paneWidth = ImGui::GetContentRegionAvail().x;
	
	if (ImGui::Button("Zoom to Content")/* || ImGui::IsMouseDoubleClicked(0)*/)
		ed::NavigateToContent();
	
	//ImGui::SameLine();
	//if (ImGui::Button("Show Flow"))
	//{
	//	for (auto& link : s_Session->s_Links)
	//		ed::Flow(link.ID);
	//}

	std::vector<ed::NodeId> selectedNodes;
	std::vector<ed::LinkId> selectedLinks;
	selectedNodes.resize(ed::GetSelectedObjectCount());
	selectedLinks.resize(ed::GetSelectedObjectCount());

	int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
	int linkCount = ed::GetSelectedLinks(selectedLinks.data(), static_cast<int>(selectedLinks.size()));

	selectedNodes.resize(nodeCount);
	selectedLinks.resize(linkCount);

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImGui::GetCursorScreenPos(),
		ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
		ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
	ImGui::Spacing(); ImGui::SameLine();
	ImGui::TextUnformatted("Nodes");
	ImGui::Indent();
	for (auto& node : s_Session->s_Nodes)
	{
		ImGui::PushID(node.ID.AsPointer());
		auto start = ImGui::GetCursorScreenPos();

		bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), node.ID) != selectedNodes.end();
		std::string nodeAndFunctionName;
		if (node.function != nullptr)
			if (node.function->getName().size())
				nodeAndFunctionName = node.Name + " \"" + node.function->getName().c_str() + "\"";
			else nodeAndFunctionName = node.Name;
		else nodeAndFunctionName = node.Name;
		if (ImGui::Selectable((nodeAndFunctionName + "##" + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer()))).c_str(), &isSelected))
		{
			if (io.KeyCtrl)
			{
				if (isSelected)
					ed::SelectNode(node.ID, true);
				else
					ed::DeselectNode(node.ID);
			}
			else
				ed::SelectNode(node.ID, false);

			ed::NavigateToSelection();
		}
		if (ImGui::IsItemHovered() && !node.State.empty())
			ImGui::SetTooltip("State: %s", node.State.c_str());

		ImGui::PopID();
	}
	ImGui::Unindent();

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImGui::GetCursorScreenPos(),
		ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
		ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
	ImGui::Spacing(); ImGui::SameLine();
	ImGui::TextUnformatted("Selection");

	//ImGui::BeginHorizontal("Selection Stats", ImVec2(paneWidth, 0));
	//ImGui::Spring();
	//if (ImGui::Button("Deselect All"))
	//	ed::ClearSelection();
	//ImGui::EndHorizontal();
	ImGui::Indent();
	for (int i = 0; i < nodeCount; ++i) ImGui::Text("Node (%p)", selectedNodes[i].AsPointer());
	for (int i = 0; i < linkCount; ++i) ImGui::Text("Link (%p)", selectedLinks[i].AsPointer());
	ImGui::Unindent();
}

void ShowMiddlePane()
{
	auto& io = ImGui::GetIO();

	//ImGui::BeginChild("NodeEditor", ImVec2(0, 0), ImGuiWindowFlags_AlwaysAutoResize);

	// This little beauty lets me split up this horrifyingly huge file.
	static statepack s;

	// ====================================================================================================================================
	// NODOS DEV - Immediate Mode node drawing.
	// ====================================================================================================================================
	ed::Begin(s_Session->beginID.c_str());

	// ====================================================================================================================================
	// NODOS DEV - draw nodes
	// newLinkPin is passed to allow highlighting of valid candiate type-safe pin destinations when link-drawing from another pin.
	// ====================================================================================================================================
	draw_nodes(s.newLinkPin);

	// ====================================================================================================================================
	// NODOS DEV - draw links
	// ====================================================================================================================================
	for (auto& link : s_Session->s_Links)
		ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);

	// ====================================================================================================================================
	// NODOS DEV - Handle link-dragging interactions in immediate mode.
	if (!s.createNewNode)
	{
		handle_link_dragging_interactions(s);
		handle_delete_interactions();

	} // Close bracket for "if (!s.createNewNode)"


	// ====================================================================================================================================
	// NODOS DEV - Handle right-click context menu spawning
	//   This section just "fires" handlers that occur later.
	//
	// ShowNodeContextMenu() - handle right-click on a node
	// ShowPinContextMenu() - handle right-click on a pin
	// ShowLinkContextMenu() - handle right-click on link
	// ShowBackgroundContextMenu() - handle right-click on graph
	//
	// Special Note about reference frame switching:
	// popup windows are not done in graph space, they're done in screen space.
	// Suspend() changes the posititiong reference frame from "graph" to "screen"
	// so, all following calls are in screen space. Then Resume() goes back to reference frame.
	// ====================================================================================================================================
	ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos());

	auto openPopupPosition = ImGui::GetMousePos();
	ed::Suspend();
	if (ed::ShowNodeContextMenu(&s.contextNodeId))
		ImGui::OpenPopup("Node Context Menu");
	else if (ed::ShowPinContextMenu(&s.contextPinId))
		ImGui::OpenPopup("Pin Context Menu");
	else if (ed::ShowLinkContextMenu(&s.contextLinkId))
		ImGui::OpenPopup("Link Context Menu");
	else if (ed::ShowBackgroundContextMenu())
	{
		ImGui::OpenPopup("Create New Node");
		s.newNodeLinkPinID = 0;
	}
	// Resume:  Calls hereafter are now in the graph reference frame.
	ed::Resume();



	// ====================================================================================================================================
	// NODOS DEV - Draw context menu bodies
	//   This section "implements" the OpenPopup() calls from the previous section.
	//
	//   "NodeContextMenu"
	//   "Pin Context Menu"
	//   "Link Context Menu"
	//   "Create New Node"
	//
	// Please read notes above about screen-space and graph-space conversion
	// implemented in Suspend() and Resume()
	// ====================================================================================================================================
	// Suspend: Calls hereafter are in screnspace.
	ed::Suspend();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	if (ImGui::BeginPopup("Node Context Menu")) // ----------------------------------------------------------------------------------------
	{
		auto node = FindNode(s.contextNodeId);

		ImGui::TextUnformatted("Node Context Menu");
		ImGui::Separator();
		if (node)
		{
			ImGui::Text("ID: %p", node->ID.AsPointer());
			ImGui::Text("Type: %s", node->Type == NodeType::Blueprint ? "Blueprint" : (node->Type == NodeType::Tree ? "Tree" : "Comment"));
			ImGui::Text("Inputs: %d", (int)node->Inputs.size());
			ImGui::Text("Outputs: %d", (int)node->Outputs.size());
		}
		else
			ImGui::Text("Unknown node: %p", s.contextNodeId.AsPointer());
		ImGui::Separator();
		if (ImGui::MenuItem("Delete"))
			ed::DeleteNode(s.contextNodeId);
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Pin Context Menu")) // ----------------------------------------------------------------------------------------
	{
		auto pin = FindPin(s.contextPinId);

		ImGui::TextUnformatted("Pin Context Menu");
		ImGui::Separator();
		if (pin)
		{
			ImGui::Text("ID: %p", pin->ID.AsPointer());
			if (pin->Node)
				ImGui::Text("Node: %p", pin->Node->ID.AsPointer());
			else
				ImGui::Text("Node: %s", "<none>");
		}
		else
			ImGui::Text("Unknown pin: %p", s.contextPinId.AsPointer());

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Link Context Menu")) // ----------------------------------------------------------------------------------------
	{
		auto link = FindLink(s.contextLinkId);

		ImGui::TextUnformatted("Link Context Menu");
		ImGui::Separator();
		if (link)
		{
			ImGui::Text("ID: %p", link->ID.AsPointer());
			ImGui::Text("From: %p", link->StartPinID.AsPointer());
			ImGui::Text("To: %p", link->EndPinID.AsPointer());
		}
		else
			ImGui::Text("Unknown link: %p", s.contextLinkId.AsPointer());
		ImGui::Separator();
		if (ImGui::MenuItem("Delete"))
			ed::DeleteLink(s.contextLinkId);
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Create New Node")) // ----------------------------------------------------------------------------------------
	{
		auto newNodePostion = openPopupPosition;
		//ImGui::SetCursorScreenPos(ImGui::GetMousePosOnOpeningCurrentPopup());

		//auto drawList = ImGui::GetWindowDrawList();
		//drawList->AddCircleFilled(ImGui::GetMousePosOnOpeningCurrentPopup(), 10.0f, 0xFFFF00FF);

		Node* node = nullptr;

		// Populate the context right click menu with all the nodes in the registry.
		for (auto nodos : s_Session->NodeRegistry) {
			if (ImGui::MenuItem(nodos.first.c_str())) {
				node = NewRegistryNode(nodos.first);
			}
		}

		// Do post-node-spawn actions here.
		if (node)
		{
			BuildNodes();

			s.createNewNode = false;

			// Move node to near the mouse location
			ed::SetNodePosition(node->ID, newNodePostion);

			// This section auto-connects a pin in your new node to a link you've dragged out
			if (auto startPin = FindPin(s.newNodeLinkPinID))
			{
				auto& pins = startPin->Kind == ed::PinKind::Input ? node->Outputs : node->Inputs;

				for (auto& pin : pins)
				{
					if (CanCreateLink(startPin, &pin))
					{
						auto endPin = &pin;
						if (startPin->Kind == ed::PinKind::Input)
							std::swap(startPin, endPin);

						s_Session->s_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
						s_Session->s_Links.back().Color = GetIconColor(startPin->Type);
						s_Session->UpdateFunctionLinks();
						break;
					}
				}
			}
		} // Done with post-node-spawn actions

		ImGui::EndPopup();
	}  // Done with ImGui::BeginPopup("Create New Node")
	else
		s.createNewNode = false;
	ImGui::PopStyleVar();
	ed::Resume();
	ed::End(); // END CALL.  We are no longer drawing nodes.

	// ====================================================================================================================================
	// NODOS DEV - outside-of-begin-end field.
	// TODO: Expand greatly and reorganize.  This is hard to find, currently.
	// ====================================================================================================================================
	// retrive selections using api
	if (ax::NodeEditor::HasSelectionChanged())
	{
		int count = ax::NodeEditor::GetSelectedObjectCount();

		// Get node selections
		ax::NodeEditor::NodeId* node_buffer = nullptr;
		int sel_nodes = ax::NodeEditor::GetSelectedNodes(nullptr, 0);
		if (sel_nodes > 0) {
			node_buffer = new ax::NodeEditor::NodeId[sel_nodes];
			ax::NodeEditor::GetSelectedNodes(node_buffer, sel_nodes);
			for (int i = 0; i < sel_nodes; i++) {
				Node* Node = FindNode(node_buffer[i]);
			}
		}

		// Get link selections
		ax::NodeEditor::LinkId* link_buffer = nullptr;
		int sel_links = ax::NodeEditor::GetSelectedLinks(nullptr, 0);
		if (sel_links > 0) {
			link_buffer = new ax::NodeEditor::LinkId[sel_links];
			ax::NodeEditor::GetSelectedLinks(link_buffer, sel_links);
			for (int i = 0; i < sel_links; i++) {
				Link* Link = FindLink(link_buffer[i]);
			}
		}

		delete[] node_buffer;
		delete[] link_buffer;
	}
}

void ShowRightPane()
{
	// Get node selections
	ax::NodeEditor::NodeId* node_buffer = nullptr;
	int sel_nodes = ax::NodeEditor::GetSelectedNodes(nullptr, 0);
	if (sel_nodes > 0) {
		node_buffer = new ax::NodeEditor::NodeId[sel_nodes];
		ax::NodeEditor::GetSelectedNodes(node_buffer, sel_nodes);
		for (int i = 0; i < sel_nodes; i++) {
			Node* Node = FindNode(node_buffer[i]);
			if (Node != nullptr) {
				if (Node->function != nullptr) {
					ImGui::Separator();
					ImGui::Text("Node \"%s\" settings:", Node->function->getName().c_str());
					ImGui::Indent();
					s_Session->NodeRegistry[Node->Name].DrawSideEditProperties(Node->function);

					ImGui::Separator();
					ImGui::NewLine();
					ImGui::Text("Node link debug info:");
					if (Node->function->getFunctionsStarter().size()) {
						ImGui::Text("functions starter:");
						for (auto functionStarter : Node->function->getFunctionsStarter())
							ImGui::Text("- \"%s\" \"%s\"", functionStarter->getType().c_str(), functionStarter->getName().c_str());
					}
					
					if (Node->function->getFunctionsResetter().size()) {
						ImGui::Text("functions resetter:");
						for (auto functionResetter : Node->function->getFunctionsResetter())
							ImGui::Text("- \"%s\" \"%s\"", functionResetter->getType().c_str(), functionResetter->getName().c_str());
					}

					if (Node->function->getFunctionsToStart().size()) {
						ImGui::Text("functions to start:");
						for (auto functionToStart : Node->function->getFunctionsToStart())
							ImGui::Text("- \"%s\" \"%s\"", functionToStart->getType().c_str(), functionToStart->getName().c_str());
					}

					if (Node->function->getFunctionsToReset().size()) {
						ImGui::Text("functions to reset:");
						for (auto functionToReset : Node->function->getFunctionsToReset())
							ImGui::Text("- \"%s\" \"%s\"", functionToReset->getType().c_str(), functionToReset->getName().c_str());
					}
				}
				else
					ImGui::TextColored((ImVec4)ImColor(255, 100, 100, 255), "error node function...");
			}
			else
				ImGui::TextColored((ImVec4)ImColor(255, 100, 100, 255), "error node...");
		}
	}
	else
		ImGui::TextDisabled("plase select node...");
}

void Frame(void)
{
    //auto& io = ImGui::GetIO();

    //ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

    ed::SetCurrentEditor(s_Session->m_Editor);
	
	ImGuiTableFlags flags = ImGuiTableFlags_BordersV;
	if (ImGui::BeginTable("Table", 3, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("NodeEditor");
		ImGui::TableSetupColumn("NodeSettings", ImGuiTableColumnFlags_WidthFixed, 300.0f);
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ShowLeftPane();
		ImGui::TableNextColumn();
		ShowMiddlePane();
		ImGui::TableNextColumn();
		ShowRightPane();
		
		ImGui::EndTable();
	}
}
}
}
