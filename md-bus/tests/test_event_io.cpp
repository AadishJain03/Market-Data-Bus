#include <gtest/gtest.h>
#include "../common/event.hpp"
#include "../common/event_io.hpp"

TEST(EventIo, SerializeTick) {
    md::Event e;
    e.h.seq = 42;
    e.h.topic = md::Topic::MD_TICK;
    e.h.ts_ns = 1234567890;

    md::Tick t;
    t.symbol = "NIFTY";
    t.pq = 22500.5;
    t.qty = 123;
    e.p = t;

    std::string s = md::serialize_event(e);

    EXPECT_NE(s.find("42"), std::string::npos);
    EXPECT_NE(s.find("1234567890"), std::string::npos);
    EXPECT_NE(s.find("MD_TICK"), std::string::npos);
    EXPECT_NE(s.find("NIFTY", std::string::npos));
}

TEST(EventIo, SerializeLog){
    md::Event e;
    e.h.seq = 7;
    e.h.topic = md::Topic::LOG;
    e.h.ts_ns = 999;

    e.p = std::string("Hello World");

    std::string s = md::serialize_event(e);
    EXPECT_NE(s.find("7"), std::string::npos);
    EXPECT_NE(s.find("999"), std::string::npos);
    EXPECT_NE(s.find("LOG"), std::string::npos);
    EXPECT_NE(s.find("Hello World"), std::string::npos);

}