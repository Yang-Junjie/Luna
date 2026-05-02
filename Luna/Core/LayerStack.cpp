#include "Layer.h"
#include "LayerStack.h"

#include <algorithm>

namespace luna {
LayerStack::~LayerStack()
{
    detachAll();
}

void LayerStack::pushLayer(std::unique_ptr<Layer> layer)
{
    m_layers.emplace(m_layers.begin() + m_layer_insert_index, std::move(layer));
    m_layer_insert_index++;
}

void LayerStack::pushOverlay(std::unique_ptr<Layer> overlay)
{
    m_layers.emplace_back(std::move(overlay));
}

void LayerStack::popLayer(Layer* layer)
{
    auto it = std::find_if(
        m_layers.begin(), m_layers.begin() + m_layer_insert_index, [layer](const std::unique_ptr<Layer>& ptr) {
            return ptr.get() == layer;
        });
    if (it != m_layers.begin() + m_layer_insert_index) {
        (*it)->onDetach();
        m_layers.erase(it);
        m_layer_insert_index--;
    }
}

void LayerStack::popOverlay(Layer* overlay)
{
    auto it = std::find_if(
        m_layers.begin() + m_layer_insert_index, m_layers.end(), [overlay](const std::unique_ptr<Layer>& ptr) {
            return ptr.get() == overlay;
        });
    if (it != m_layers.end()) {
        (*it)->onDetach();
        m_layers.erase(it);
    }
}

void LayerStack::detachAll()
{
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        if (*it) {
            (*it)->onDetach();
        }
    }

    m_layers.clear();
    m_layer_insert_index = 0;
}
} // namespace luna
