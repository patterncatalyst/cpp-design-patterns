#pragma once

#include <memory>
#include <string>
#include <vector>

namespace warehouse {

// A sub-manifest in a shipment's bill-of-lading tree: a root manifest can
// be split into sub-manifests as it moves through a sorting hub, and each
// sub-manifest can be split again. A parent owns its children outright
// through std::shared_ptr (more than one caller may hold a child alive
// independently of the parent, e.g. while a dock worker is still scanning
// it), but a child must not own its parent back the same way: a
// std::shared_ptr in both directions would keep both objects' reference
// counts above zero forever, even after every external reference is gone,
// because each one is the thing keeping the other alive. ManifestNode
// breaks that cycle by making the child-to-parent edge a std::weak_ptr: a
// non-owning reference that observes whether the parent is still alive
// without counting toward its lifetime.
class ManifestNode : public std::enable_shared_from_this<ManifestNode> {
public:
    explicit ManifestNode(std::string manifest_id);

    // Creates a child sub-manifest, links it to this node, and returns it.
    // This node must already be owned by a shared_ptr (see
    // make_root_manifest below) before this is called, because
    // shared_from_this() requires it.
    std::shared_ptr<ManifestNode> add_child(std::string manifest_id);

    // Non-owning: locking this after the parent has been destroyed returns
    // an empty shared_ptr rather than extending the parent's lifetime.
    std::weak_ptr<ManifestNode> parent() const;

    const std::vector<std::shared_ptr<ManifestNode>>& children() const;
    const std::string& manifest_id() const;

private:
    std::string manifest_id_;
    std::weak_ptr<ManifestNode> parent_;
    std::vector<std::shared_ptr<ManifestNode>> children_;
};

// Factory for a root manifest. ManifestNode cannot safely construct its
// own first shared_ptr from inside a constructor (shared_from_this()
// requires an object already owned by a shared_ptr), so the root is always
// created through this function rather than a bare constructor call.
std::shared_ptr<ManifestNode> make_root_manifest(std::string manifest_id);

}  // namespace warehouse
