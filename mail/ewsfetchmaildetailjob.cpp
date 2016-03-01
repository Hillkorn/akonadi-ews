/*  This file is part of Akonadi EWS Resource
    Copyright (C) 2015-2016 Krzysztof Nowicki <krissn@op.pl>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "ewsfetchmaildetailjob.h"

#include <KMime/Message>
#include <KMime/HeaderParsing>
#include <Akonadi/KMime/MessageFlags>

#include "ewsitemshape.h"
#include "ewsgetitemrequest.h"
#include "ewsmailbox.h"
#include "ewsclient_debug.h"

using namespace Akonadi;

static const EwsPropertyField propPidFlagStatus(0x1090, EwsPropTypeInteger);
static const EwsPropertyField propPidFlagIconIndex(0x1080, EwsPropTypeInteger);

EwsFetchMailDetailJob::EwsFetchMailDetailJob(EwsClient &client, QObject *parent, const Akonadi::Collection &collection)
    : EwsFetchItemDetailJob(client, parent, collection)
{
    EwsItemShape shape(EwsShapeIdOnly);
    shape << EwsPropertyField("item:Subject");
    shape << EwsPropertyField("item:Importance");
    shape << EwsPropertyField("message:From");
    shape << EwsPropertyField("message:ToRecipients");
    shape << EwsPropertyField("message:CcRecipients");
    shape << EwsPropertyField("message:BccRecipients");
    shape << EwsPropertyField("message:IsRead");
    shape << EwsPropertyField("item:HasAttachments");
    shape << EwsPropertyField("item:Categories");
    shape << EwsPropertyField("item:DateTimeReceived");
    shape << EwsPropertyField("item:InReplyTo");
    shape << EwsPropertyField("message:References");
    shape << EwsPropertyField("message:ReplyTo");
    shape << EwsPropertyField("message:InternetMessageId");
    shape << propPidFlagStatus;
    shape << propPidFlagIconIndex;
    mRequest->setItemShape(shape);
}


EwsFetchMailDetailJob::~EwsFetchMailDetailJob()
{
}

void EwsFetchMailDetailJob::processItems(const QList<EwsGetItemRequest::Response> &responses)
{
    Item::List::iterator it = mChangedItems.begin();

    Q_FOREACH(const EwsGetItemRequest::Response &resp, responses) {
        Item &item = *it;

        if (!resp.isSuccess()) {
            qCWarningNC(EWSRES_LOG) << QStringLiteral("Failed to fetch item %1").arg(item.remoteId());
            continue;
        }

        const EwsItem &ewsItem = resp.item();
        KMime::Message::Ptr msg(new KMime::Message);

        // Rebuild the message headers
        QVariant v = ewsItem[EwsItemFieldSubject];
        if (Q_LIKELY(v.isValid())) {
            msg->subject()->fromUnicodeString(v.toString(), "utf-8");
        }

        v = ewsItem[EwsItemFieldFrom];
        if (Q_LIKELY(v.isValid())) {
            EwsMailbox mbox = v.value<EwsMailbox>();
            msg->from()->addAddress(mbox);
        }

        v = ewsItem[EwsItemFieldToRecipients];
        if (Q_LIKELY(v.isValid())) {
            EwsMailbox::List mboxList = v.value<EwsMailbox::List>();
            QStringList addrList;
            Q_FOREACH(const EwsMailbox &mbox, mboxList) {
                msg->to()->addAddress(mbox);
            }
        }

        v = ewsItem[EwsItemFieldCcRecipients];
        if (Q_LIKELY(v.isValid())) {
            EwsMailbox::List mboxList = v.value<EwsMailbox::List>();
            QStringList addrList;
            Q_FOREACH(const EwsMailbox &mbox, mboxList) {
                msg->cc()->addAddress(mbox);
            }
        }

        v = ewsItem[EwsItemFieldBccRecipients];
        if (v.isValid()) {
            EwsMailbox::List mboxList = v.value<EwsMailbox::List>();
            QStringList addrList;
            Q_FOREACH(const EwsMailbox &mbox, mboxList) {
                msg->bcc()->addAddress(mbox);
            }
        }

        v = ewsItem[EwsItemFieldInternetMessageId];
        if (v.isValid()) {
            msg->messageID()->from7BitString(v.toString().toAscii());
        }

        v = ewsItem[EwsItemFieldInReplyTo];
        if (v.isValid()) {
            msg->inReplyTo()->from7BitString(v.toString().toAscii());
        }

        v = ewsItem[EwsItemFieldDateTimeReceived];
        if (v.isValid()) {
            msg->date()->setDateTime(v.toDateTime());
        }

        v = ewsItem[EwsItemFieldReferences];
        if (v.isValid()) {
            msg->references()->from7BitString(v.toString().toAscii());
        }

        v = ewsItem[EwsItemFieldReplyTo];
        if (v.isValid()) {
            EwsMailbox mbox = v.value<EwsMailbox>();
            msg->replyTo()->addAddress(mbox);
        }

        msg->assemble();
        item.setPayload(KMime::Message::Ptr(msg));

        v = ewsItem[EwsItemFieldIsRead];
        if (v.isValid()) {
            if (v.toBool()) {
                item.setFlag(MessageFlags::Seen);
            }
            else {
                item.clearFlag(MessageFlags::Seen);
            }
        }

        v = ewsItem[EwsItemFieldHasAttachments];
        if (v.isValid()) {
            if (v.toBool()) {
                item.setFlag(MessageFlags::HasAttachment);
            }
            else {
                item.clearFlag(MessageFlags::HasAttachment);
            }
        }

        v = ewsItem[EwsItemFieldIsFromMe];
        if (v.isValid()) {
            if (v.toBool()) {
                item.setFlag(MessageFlags::Sent);
            }
            else {
                item.clearFlag(MessageFlags::Sent);
            }
        }

        QStringRef flagProp = ewsItem[propPidFlagStatus];
        if (!flagProp.isNull() && (flagProp.toUInt() == 2)) {
            item.setFlag(MessageFlags::Flagged);
        }
        else {
            item.clearFlag(MessageFlags::Flagged);
        }

        it++;
    }

    qDebug() << __FUNCTION__ << "done";

    emitResult();
}
