/*
 *
 * (C) 2014 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

namespace SASL
{
	struct Message
	{
		Anope::string source;
		Anope::string target;
		Anope::string type;
		Anope::string data;
		Anope::string ext;
	};

	class Mechanism;
	struct Session;

	class Service : public ::Service
	{
	 public:
		Service(Module *o) : ::Service(o, "SASL::Service", "sasl") { }

		virtual void ProcessMessage(const Message &) = 0;

		virtual Anope::string GetAgent() = 0;

		virtual Session* GetSession(const Anope::string &uid) = 0;

		virtual void SendMessage(SASL::Session *session, const Anope::string &type, const Anope::string &data) = 0;

		virtual void Succeed(Session *, NickServ::Account *) = 0;
		virtual void Fail(Session *) = 0;
		virtual void SendMechs(Session *) = 0;
		virtual void DeleteSessions(Mechanism *, bool = false) = 0;
		virtual void RemoveSession(Session *) = 0;
	};

	static ServiceReference<SASL::Service> sasl("SASL::Service", "sasl");

	struct Session
	{
		time_t created;
		Anope::string uid;
		Reference<Mechanism> mech;

		Session(Mechanism *m, const Anope::string &u) : created(Anope::CurTime), uid(u), mech(m) { }
		virtual ~Session()
		{
			if (sasl)
				sasl->RemoveSession(this);
		}
	};

	/* PLAIN, EXTERNAL, etc */
	class Mechanism : public ::Service
	{
	 public:
		Mechanism(Module *o, const Anope::string &sname) : Service(o, "SASL::Mechanism", sname) { }

		virtual Session* CreateSession(const Anope::string &uid) { return new Session(this, uid); }

		virtual void ProcessMessage(Session *session, const Message &) = 0;

		virtual ~Mechanism()
		{
			if (sasl)
				sasl->DeleteSessions(this, true);
		}
	};

	class IdentifyRequestListener : public NickServ::IdentifyRequestListener
	{
		Anope::string uid;

	 public:
		IdentifyRequestListener(const Anope::string &id) : uid(id) { }

		void OnSuccess(NickServ::IdentifyRequest *req) override
		{
			if (!sasl)
				return;

			NickServ::Nick *na = NickServ::FindNick(req->GetAccount());
			if (!na || na->nc->HasExt("NS_SUSPENDED"))
				return OnFail(req);

			Session *s = sasl->GetSession(uid);
			if (s)
			{
				Log(Config->GetClient("NickServ")) << "A user identified to account " << req->GetAccount() << " using SASL";
				sasl->Succeed(s, na->nc);
				delete s;
			}
		}

		void OnFail(NickServ::IdentifyRequest *req) override
		{
			if (!sasl)
				return;

			Session *s = sasl->GetSession(uid);
			if (s)
			{
				sasl->Fail(s);
				delete s;
			}

			Anope::string accountstatus;
			NickServ::Nick *na = NickServ::FindNick(req->GetAccount());
			if (!na)
				accountstatus = "nonexistent ";
			else if (na->nc->HasExt("NS_SUSPENDED"))
				accountstatus = "suspended ";

			Log(Config->GetClient("NickServ")) << "A user failed to identify for " << accountstatus << "account " << req->GetAccount() << " using SASL";
		}
	};
}
