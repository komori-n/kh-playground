COMPILER = clang++

GTEST_DIR=../../../../googletest/googletest

CPPFLAGS += -isystem $(GTEST_DIR)/include
CPPFLAGS += -std=c++17 -fno-exceptions -fno-rtti -Wunused-variable -Wextra -Ofast -MMD -MP -fpermissive
CPPFLAGS  += -DUNICODE -DNO_EXCEPTIONS
CPPFLAGS  += -fprofile-arcs -ftest-coverage
CPPFLAGS  += -g3 -ggdb
WCPPFLAGS  =
CXXFLAGS += -g -Wall -Wextra -pthread
LDFLAGS += -lpthread
LDFLAGS += -v --coverage
LTOFLAGS = -flto
OBJDIR = ../../../../obj/source/engine/user-engine/tests

INCLUDE = -I$(GTEST_DIR)/include/gtest/*.h \
          -I$(GTEST_DIR)/include/gtest/internal/*.h \
          -I../../../ \
          -I../

ifeq ($(shell uname),Linux)
    CPPFLAGS += -D_LINUX
endif
TARGET = kh-test

SOURCES  = \
    ../../../types.cpp                                                          \
    ../../../bitboard.cpp                                                       \
    ../../../misc.cpp                                                           \
    ../../../movegen.cpp                                                        \
    ../../../position.cpp                                                       \
    ../../../usi.cpp                                                            \
    ../../../usi_option.cpp                                                     \
    ../../../thread.cpp                                                         \
    ../../../tt.cpp                                                             \
    ../../../movepick.cpp                                                       \
    ../../../timeman.cpp                                                        \
    ../../../book/book.cpp                                                      \
    ../../../book/apery_book.cpp                                                \
    ../../../extra/bitop.cpp                                                    \
    ../../../extra/long_effect.cpp                                              \
    ../../../extra/sfen_packer.cpp                                              \
    ../../../extra/super_sort.cpp                                               \
    ../../../mate/mate.cpp                                                      \
    ../../../mate/mate1ply_without_effect.cpp                                   \
    ../../../mate/mate1ply_with_effect.cpp                                      \
    ../../../mate/mate_solver.cpp                                               \
    ../../../eval/evaluate_bona_piece.cpp                                       \
    ../../../eval/evaluate.cpp                                                  \
    ../../../eval/evaluate_io.cpp                                               \
    ../../../eval/evaluate_mir_inv_tools.cpp                                    \
    ../../../eval/material/evaluate_material.cpp                                \
    ../../../testcmd/unit_test.cpp                                              \
    ../../../testcmd/mate_test_cmd.cpp                                          \
    ../../../testcmd/normal_test_cmd.cpp                                        \
    ../../../testcmd/benchmark.cpp

SOURCES += dummy_engine.cpp \
    ../path_keys.cpp \
    ../hands.cpp
SOURCES += $(wildcard *_test.cpp) test_main.cpp

CPPFLAGS += -DTARGET_CPU=\"$(TARGET_CPU)\"
CPPFLAGS += -DUSER_ENGINE

OBJECTS  = $(addprefix $(OBJDIR)/, $(SOURCES:.cpp=.o))
DEPENDS  = $(OBJECTS:.o=.d)

LIBS    = $(OBJDIR)/gtest.a

all: $(TARGET)
.PHONY : all test coverage clean

test: $(TARGET)
	rm -rf $(OBJECTS:.o=.gcda)
	./$(TARGET) || exit 1

$(TARGET): $(OBJECTS) $(LIBS)
	$(COMPILER) -o $@ $^ $(LDFLAGS) $(CPPFLAGS)

$(OBJDIR)/%.o: %.cpp
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(COMPILER) $(CPPFLAGS) $(INCLUDE) -o $@ -c $<

GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

$(OBJDIR)/gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest-all.cc -o $@

$(OBJDIR)/gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest_main.cc -o $@

$(OBJDIR)/gtest.a : $(OBJDIR)/gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

$(OBJDIR)/gtest_main.a : $(OBJDIR)/gtest-all.o $(OBJDIR)/gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

coverage: test
	lcov --directory ../../../../obj --base-directory . --capture -o cov.info
	lcov -e cov.info -o cov.info "**/user-engine/*"
	lcov -r cov.info -o cov.info "**/tests/*"
	genhtml cov.info -o coverage-html

clean:
	rm -rf $(OBJECTS) $(DEPENDS) $(TARGET) ${OBJECTS:.o=.gcda} ${OBJDIR}

-include $(DEPENDS)