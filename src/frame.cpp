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

static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}
void ShowLeftPane(float paneWidth)
{
	auto& io = ImGui::GetIO();

	ImGui::BeginChild("Selection", ImVec2(paneWidth, 0));

	paneWidth = ImGui::GetContentRegionAvail().x;
	
	if (ImGui::Button("Zoom to Content"))
		ed::NavigateToContent();
	
	ImGui::SameLine();
	if (ImGui::Button("Show Flow"))
	{
		for (auto& link : s_Session->s_Links)
			ed::Flow(link.ID);
	}

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
		if (ImGui::Selectable((node.Name + "##" + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer()))).c_str(), &isSelected))
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

	static int changeCount = 0;

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImGui::GetCursorScreenPos(),
		ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
		ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
	ImGui::Spacing(); ImGui::SameLine();
	ImGui::TextUnformatted("Selection");

	ImGui::BeginHorizontal("Selection Stats", ImVec2(paneWidth, 0));
	ImGui::Text("Changed %d time%s", changeCount, changeCount > 1 ? "s" : "");
	ImGui::Spring();
	if (ImGui::Button("Deselect All"))
		ed::ClearSelection();
	ImGui::EndHorizontal();
	ImGui::Indent();
	for (int i = 0; i < nodeCount; ++i) ImGui::Text("Node (%p)", selectedNodes[i].AsPointer());
	for (int i = 0; i < linkCount; ++i) ImGui::Text("Link (%p)", selectedLinks[i].AsPointer());
	ImGui::Unindent();

	if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
		for (auto& link : s_Session->s_Links)
			ed::Flow(link.ID);

	if (ed::HasSelectionChanged())
		++changeCount;

	ImGui::EndChild();
}

void Frame(void)
{
	std::lock_guard<std::mutex> lock(*s_Session->contextMtx); // lock context...

    auto& io = ImGui::GetIO();

    //ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

    ed::SetCurrentEditor(s_Session->m_Editor);

    //auto& style = ImGui::GetStyle();

# if 0
    {
        for (auto x = -io.DisplaySize.y; x < io.DisplaySize.x; x += 10.0f)
        {
            ImGui::GetWindowDrawList()->AddLine(ImVec2(x, 0), ImVec2(x + io.DisplaySize.y, io.DisplaySize.y),
                IM_COL32(255, 255, 0, 255));
        }
    }
# endif



    static float leftPaneWidth  = 400.0f;
    static float rightPaneWidth = 800.0f;
    Splitter(true, 4.0f, &leftPaneWidth, &rightPaneWidth, 50.0f, 50.0f);

    ShowLeftPane(leftPaneWidth - 4.0f);

    ImGui::SameLine(0.0f, 12.0f);

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
# if 1
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
        s.newNodeLinkPin = nullptr;
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
        for(auto nodos: s_Session->NodeRegistry){
            if (ImGui::MenuItem(nodos.first.c_str())){
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
            if (auto startPin = s.newNodeLinkPin)
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
# endif

/*
    cubic_bezier_t c;
    c.p0 = pointf(100, 600);
    c.p1 = pointf(300, 1200);
    c.p2 = pointf(500, 100);
    c.p3 = pointf(900, 600);

    auto drawList = ImGui::GetWindowDrawList();
    auto offset_radius = 15.0f;
    auto acceptPoint = [drawList, offset_radius](const bezier_subdivide_result_t& r)
    {
        drawList->AddCircle(to_imvec(r.point), 4.0f, IM_COL32(255, 0, 255, 255));

        auto nt = r.tangent.normalized();
        nt = pointf(-nt.y, nt.x);

        drawList->AddLine(to_imvec(r.point), to_imvec(r.point + nt * offset_radius), IM_COL32(255, 0, 0, 255), 1.0f);
    };

    drawList->AddBezierCurve(to_imvec(c.p0), to_imvec(c.p1), to_imvec(c.p2), to_imvec(c.p3), IM_COL32(255, 255, 255, 255), 1.0f);
    cubic_bezier_subdivide(acceptPoint, c);
*/
    ed::End(); // END CALL.  We are no longer drawing nodes.

    // ====================================================================================================================================
    // NODOS DEV - outside-of-begin-end field.
    // TODO: Expand greatly and reorganize.  This is hard to find, currently.
    // ====================================================================================================================================
    // retrive selections using api
    if (ax::NodeEditor::HasSelectionChanged())
    {
        int count =  ax::NodeEditor::GetSelectedObjectCount();

        // Get node selections
        ax::NodeEditor::NodeId* node_buffer = nullptr;
        int sel_nodes = ax::NodeEditor::GetSelectedNodes(nullptr,0);
        if (sel_nodes > 0) {
            node_buffer = new ax::NodeEditor::NodeId[sel_nodes];
            ax::NodeEditor::GetSelectedNodes(node_buffer,sel_nodes);
            for (int i = 0; i < sel_nodes; i++) {
                Node* Node = FindNode(node_buffer[i]);
            }
        }


        // Get link selections
        ax::NodeEditor::LinkId* link_buffer = nullptr;
        int sel_links = ax::NodeEditor::GetSelectedLinks(nullptr,0);
        if (sel_links > 0) {
            link_buffer = new ax::NodeEditor::LinkId[sel_links];
            ax::NodeEditor::GetSelectedLinks(link_buffer,sel_links);
            for (int i = 0; i < sel_links; i++) {
                Link* Link = FindLink(link_buffer[i]);
            }
        }

        // int sel_links = ax::NodeEditor::GetSelectedLinks(LinkId* links, int size);
        //bool IsNodeSelected(NodeId nodeId);
        //bool IsLinkSelected(LinkId linkId);


        delete[] node_buffer;
        delete[] link_buffer;

    }


    //ImGui::ShowTestWindow();
    //ImGui::ShowMetricsWindow();
}
}
}
