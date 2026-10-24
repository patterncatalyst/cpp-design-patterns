#include "manifest_tree.hpp"

#include <utility>

namespace warehouse {

ManifestNode::ManifestNode(std::string manifest_id) : manifest_id_(std::move(manifest_id)) {}

std::shared_ptr<ManifestNode> ManifestNode::add_child(std::string manifest_id) {
    auto child = std::make_shared<ManifestNode>(std::move(manifest_id));
    child->parent_ = weak_from_this();
    children_.push_back(child);
    return child;
}

std::weak_ptr<ManifestNode> ManifestNode::parent() const {
    return parent_;
}

const std::vector<std::shared_ptr<ManifestNode>>& ManifestNode::children() const {
    return children_;
}

const std::string& ManifestNode::manifest_id() const {
    return manifest_id_;
}

std::shared_ptr<ManifestNode> make_root_manifest(std::string manifest_id) {
    return std::make_shared<ManifestNode>(std::move(manifest_id));
}

}  // namespace warehouse
