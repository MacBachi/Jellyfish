# Host build of the mode current simulation; see power_sim.cpp.
#   make            default ring
#   make RING=39    39-LED ring
FW := ../..
CXX ?= c++
CXXFLAGS := -std=c++17 -O1 -Wno-deprecated -I stubs -I $(FW)
ifdef RING
CXXFLAGS += -DJELL_RING_LEDS=$(RING)
endif
SRC := power_sim.cpp $(FW)/jell_effects.cpp $(FW)/jell_field.cpp $(FW)/jell_canvas.cpp

power_sim: $(SRC) stubs/pico_stub.h $(FW)/jell_canvas.hpp $(FW)/jell_led.hpp $(FW)/jell_config.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

run: power_sim
	./power_sim

clean:
	rm -f power_sim

.PHONY: run clean
