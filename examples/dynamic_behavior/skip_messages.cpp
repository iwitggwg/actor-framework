#include "caf/all.hpp"
using namespace caf;

using idle_atom = atom_constant<atom("idle")>;
using request_atom = atom_constant<atom("request")>;
using response_atom = atom_constant<atom("response")>;

behavior server(event_based_actor* self) {
  auto die = [=] { self->quit(exit_reason::unexpected_message); };
  return {
    [=](idle_atom, const actor& worker) {
      self->become (
        keep_behavior,
        [=](request_atom) {
          self->forward_to(worker);
          self->unbecome();
        },
        [=](idle_atom) {
          return skip_message();
        },
        others >> die
      );
    },
    [=](request_atom) {
      return skip_message();
    },
    others >> die
  };
}

behavior client(event_based_actor* self, const actor& serv) {
  self->link_to(serv);
  self->send(serv, idle_atom::value, self);
  return {
    [=](request_atom) {
      self->send(serv, idle_atom::value, self);
      return response_atom::value;
    }
  };
}

int main() {
  actor_system system;
  auto serv = system.spawn(server);
  auto worker = system.spawn(client, serv);
  scoped_actor self{system};
  self->request(serv, std::chrono::seconds(10),
                request_atom::value).receive([&](response_atom) {
    aout(self) << "received response from "
               << (self->current_sender() == worker ? "worker\n" : "server\n");
  });
  self->send_exit(serv, exit_reason::user_shutdown);
}
