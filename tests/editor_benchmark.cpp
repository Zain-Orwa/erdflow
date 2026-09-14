#include "app/desktop/diagram_view.hpp"
#include "infrastructure/project_store.hpp"

#include <QApplication>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <chrono>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    using namespace erdflow;
    using Clock = std::chrono::steady_clock;
    auto milliseconds = [](Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    };
    auto check = [](const application::EditResult& result) {
        if (!result) throw std::runtime_error(result.error);
    };
    try {
        infrastructure::QtIdGenerator ids;
        application::Editor editor(ids);
        std::vector<domain::EntityId> entities;
        auto start = Clock::now();
        for (int i = 0; i < 1000; ++i) {
            const auto result = editor.create_entity("Entity " + std::to_string(i),
                {(i % 40) * 240.0, (i / 40) * 180.0, 160, 80});
            check(result);
            entities.push_back(std::get<domain::EntityId>(*result.created));
        }
        std::cout << "create_1000_entities_ms=" << milliseconds(start) << '\n';
        for (int i = 0; i < 250; ++i) {
            const auto result = editor.create_relationship("Relationship " + std::to_string(i),
                {(i % 40) * 240.0 + 130, (i / 40) * 180.0 + 100, 120, 70});
            check(result);
            const auto relationship = std::get<domain::RelationshipId>(*result.created);
            check(editor.connect(relationship, entities[static_cast<std::size_t>(i)]));
            check(editor.connect(relationship, entities[static_cast<std::size_t>(i + 1)]));
        }
        start = Clock::now();
        desktop::DiagramView view(editor);
        view.resize(1000, 700);
        view.show();
        QApplication::processEvents();
        std::cout << "project_1250_nodes_500_edges_ms=" << milliseconds(start) << '\n';
        view.select_elements({entities.front()});
        view.centerOn(80, 40);
        QApplication::processEvents();
        const auto local = view.mapFromScene(QPointF(80, 40));
        auto send = [&](QEvent::Type type, QPoint point, Qt::MouseButton button, Qt::MouseButtons buttons) {
            QMouseEvent event(type, QPointF(point), QPointF(view.viewport()->mapToGlobal(point)), button, buttons, Qt::NoModifier);
            QApplication::sendEvent(view.viewport(), &event);
            QApplication::processEvents();
        };
        send(QEvent::MouseButtonPress, local, Qt::LeftButton, Qt::LeftButton);
        start = Clock::now();
        for (int i = 1; i <= 100; ++i) send(QEvent::MouseMove, local + QPoint(i, i / 2), Qt::NoButton, Qt::LeftButton);
        std::cout << "offscreen_drag_100_events_ms=" << milliseconds(start) << '\n';
        send(QEvent::MouseButtonRelease, local + QPoint(100, 50), Qt::LeftButton, Qt::NoButton);
        start = Clock::now();
        check(editor.rename(entities.front(), "Renamed"));
        view.synchronize();
        std::cout << "rename_validate_synchronize_ms=" << milliseconds(start) << '\n';
        start = Clock::now();
        const auto encoded = infrastructure::ErdxProjectStore::encode(editor.project());
        const auto decoded = infrastructure::ErdxProjectStore::decode(encoded);
        if (!decoded || *decoded.project != editor.project()) throw std::runtime_error("Benchmark roundtrip failed");
        std::cout << "encode_decode_validate_ms=" << milliseconds(start) << '\n';
        std::cout << "file_bytes=" << encoded.size() << " history_estimated_bytes=" << editor.history_bytes() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
