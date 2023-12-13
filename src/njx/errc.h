#ifndef MSGSTREAM_ERRC_H
#define MSGSTREAM_ERRC_H

{% for errc in errorCodes %}
#define {{ errc.name }} {{ errc.value }}
{% endfor %}

#endif
