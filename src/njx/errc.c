#include "msgstream.h"

const char *msgstream_errname(int ec) {
	switch (ec) {
		{% for err in errorCodes %}
		case {{ err.name }}:
			return "{{ err.name }}";
			break;
		{% endfor %}
		default:
			return "(Unknown msgstream error code)";
	}
}

const char *msgstream_errstr(int ec) {
	switch (ec) {
		{% for err in errorCodes %}
		case {{ err.name }}:
			return "{{ err.msg }}";
			break;
		{% endfor %}
		default:
			return "(Unknown msgstream error code)";
	}
}
